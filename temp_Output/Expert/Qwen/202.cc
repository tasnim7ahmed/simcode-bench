#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/csma-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/netanim-module.h"
#include "ns3/energy-module.h"
#include "ns3/lr-wpan-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WSN_Simulation");

int main(int argc, char *argv[])
{
    bool enableFlowMonitor = true;
    uint32_t packetSize = 128;
    Time interPacketInterval = Seconds(5.0);
    double simulationTime = 60.0;

    NodeContainer nodes;
    nodes.Create(6);

    LrWpanHelper lrWpanHelper;
    NetDeviceContainer devices = lrWpanHelper.Install(nodes);

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
    onoff.SetAttribute("PacketSize", UintegerValue(packetSize));

    ApplicationContainer sourceApps;
    for (uint32_t i = 1; i < nodes.GetN(); ++i)
    {
        sourceApps.Add(onoff.Install(nodes.Get(i)));
    }

    sourceApps.Start(Seconds(1.0));
    sourceApps.Stop(Seconds(simulationTime));

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor;
    if (enableFlowMonitor)
    {
        monitor = flowmon.InstallAll();
    }

    AsciiTraceHelper asciiTraceHelper;
    lrWpanHelper.EnableAsciiAll(asciiTraceHelper.CreateFileStream("wsn-lrwpan.tr"));

    AnimationInterface anim("wsn-animation.xml");
    for (uint32_t i = 0; i < nodes.GetN(); ++i)
    {
        anim.UpdateNodeDescription(nodes.Get(i), "Node-" + std::to_string(i));
        anim.UpdateNodeColor(nodes.Get(i), 255, 0, 0);
    }
    anim.UpdateNodeDescription(nodes.Get(0), "Sink-Node");
    anim.UpdateNodeColor(nodes.Get(0), 0, 0, 255);

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    if (enableFlowMonitor)
    {
        monitor->CheckForLostPackets();
        FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
        double totalRx = 0, totalTx = 0, totalDelay = 0, totalBytes = 0;
        for (auto itr = stats.begin(); itr != stats.end(); ++itr)
        {
            FlowId id = itr->first;
            FlowMonitor::FlowStats s = itr->second;
            if (id == 0) continue;
            totalTx += s.txPackets;
            totalRx += s.rxPackets;
            totalDelay += s.delaySum.GetSeconds();
            totalBytes += s.rxBytes;
        }

        double pdr = (totalTx > 0) ? (totalRx / totalTx) : 0;
        double avgDelay = (totalRx > 0) ? (totalDelay / totalRx) : 0;
        double throughput = (simulationTime > 0) ? ((totalBytes * 8) / simulationTime / 1000) : 0;

        NS_LOG_UNCOND("Performance Metrics:");
        NS_LOG_UNCOND("Packet Delivery Ratio: " << pdr);
        NS_LOG_UNCOND("Average End-to-End Delay: " << avgDelay << "s");
        NS_LOG_UNCOND("Throughput: " << throughput << " kbps");
    }

    Simulator::Destroy();
    return 0;
}