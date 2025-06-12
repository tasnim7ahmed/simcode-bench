#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/energy-module.h"
#include "ns3/lr-wpan-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WirelessSensorNetwork");

int main(int argc, char *argv[]) {
    uint32_t numNodes = 6;
    double simulationTime = 10.0;
    double dataInterval = 1.0;
    std::string animFile = "wsn-animation.xml";

    CommandLine cmd(__FILE__);
    cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
    cmd.AddValue("dataInterval", "Interval between data packets (seconds)", dataInterval);
    cmd.Parse(argc, argv);

    NodeContainer nodes;
    nodes.Create(numNodes);

    LrWpanHelper lrWpan;
    NetDeviceContainer devices = lrWpan.Install(nodes);

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(10.0),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(6),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    InternetStackHelper internet;
    internet.Install(nodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    uint16_t sinkPort = 9;
    Address sinkAddress(InetSocketAddress(interfaces.GetAddress(0), sinkPort));

    PacketSinkHelper packetSinkHelper("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
    ApplicationContainer sinkApps = packetSinkHelper.Install(nodes.Get(0));
    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(Seconds(simulationTime));

    OnOffHelper onoff("ns3::UdpSocketFactory", sinkAddress);
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onoff.SetAttribute("DataRate", DataRateValue(DataRate("1kbps")));
    onoff.SetAttribute("PacketSize", UintegerValue(64));

    ApplicationContainer sourceApps;
    for (uint32_t i = 1; i < numNodes; ++i) {
        onoff.SetAttribute("StartTime", TimeValue(Seconds(dataInterval * (i - 1))));
        sourceApps.Add(onoff.Install(nodes.Get(i)));
    }

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    lrWpan.EnableLogComponents();
    LogComponentEnable("PacketSink", LOG_LEVEL_INFO);

    Config::SetDefault("ns3::LrWpanPhy::ChannelPage", UintegerValue(0));
    Config::SetDefault("ns3::LrWpanPhy::PibAttribute_11b_OQPSK", BooleanValue(true));
    Config::SetDefault("ns3::LrWpanCsmaCa::SlottedCsmaCa", BooleanValue(false));

    AnimationInterface anim(animFile);
    for (uint32_t i = 0; i < numNodes; ++i) {
        anim.UpdateNodeDescription(nodes.Get(i), "Node-" + std::to_string(i));
    }
    anim.UpdateNodeColor(nodes.Get(0), 255, 0, 0);

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

    double totalRx = 0;
    double totalTx = 0;
    double delaySum = Seconds(0).GetSeconds();

    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator iter = stats.begin(); iter != stats.end(); ++iter) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(iter->first);
        if (t.destinationPort == sinkPort) {
            totalRx += iter->second.rxPackets;
            totalTx += iter->second.txPackets;
            delaySum += iter->second.delaySum.GetSeconds();
        }
    }

    double pdr = (totalTx > 0) ? (totalRx / totalTx) : 0.0;
    double avgDelay = (totalRx > 0) ? (delaySum / totalRx) : 0.0;
    double throughput = (totalRx * 64 * 8) / simulationTime / 1000; // kbps

    NS_LOG_UNCOND("Packet Delivery Ratio: " << pdr);
    NS_LOG_UNCOND("Average End-to-End Delay: " << avgDelay << " s");
    NS_LOG_UNCOND("Throughput: " << throughput << " kbps");

    Simulator::Destroy();
    return 0;
}