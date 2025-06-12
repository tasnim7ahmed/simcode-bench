#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/aodv-module.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WirelessAdHocAODVSimulation");

int main(int argc, char *argv[]) {
    uint32_t numNodes = 4;
    double simTime = 20.0;
    double distance = 500.0;

    CommandLine cmd(__FILE__);
    cmd.AddValue("numNodes", "Number of nodes in the simulation", numNodes);
    cmd.AddValue("simTime", "Total simulation time (seconds)", simTime);
    cmd.Parse(argc, argv);

    NodeContainer nodes;
    nodes.Create(numNodes);

    YansWifiPhyHelper wifiPhy;
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211a);
    wifi.SetRemoteStationManager("ns3::ArfWifiManager");

    NqosWifiMacHelper wifiMac = NqosWifiMacHelper::Default();
    wifiMac.SetType("ns3::AdhocWifiMac");

    WifiChannelHelper wifiChannel = WifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());

    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(50.0),
                                  "DeltaY", DoubleValue(50.0),
                                  "GridWidth", UintegerValue(2),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(0, distance, 0, distance)));
    mobility.Install(nodes);

    AodvHelper aodv;
    InternetStackHelper stack;
    stack.SetRoutingHelper(aodv);
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    Config::SetDefault("ns3::Ipv4L3Protocol::OutputQueueSize", StringValue("1000"));

    ApplicationContainer apps;
    uint16_t port = 9;

    for (uint32_t i = 0; i < numNodes; ++i) {
        for (uint32_t j = 0; j < numNodes; ++j) {
            if (i != j) {
                UdpEchoServerHelper echoServer(port);
                apps = echoServer.Install(nodes.Get(j));
                apps.Start(Seconds(1.0));
                apps.Stop(Seconds(simTime));

                UdpEchoClientHelper echoClient(interfaces.GetAddress(j), port);
                echoClient.SetAttribute("MaxPackets", UintegerValue(1000));
                echoClient.SetAttribute("Interval", TimeValue(Seconds(0.1)));
                echoClient.SetAttribute("PacketSize", UintegerValue(512));

                apps = echoClient.Install(nodes.Get(i));
                apps.Start(Seconds(2.0));
                apps.Stop(Seconds(simTime));
            }
        }
    }

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    AsciiTraceHelper asciiTraceHelper;
    wifiPhy.EnableAsciiAll(asciiTraceHelper.CreateFileStream("wireless-adhoc-aodv.tr"));

    AnimationInterface anim("wireless-adhoc-aodv.xml");
    anim.SetMobilityPollInterval(Seconds(0.5));

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();

    monitor->CheckForLostPackets();

    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator iter = stats.begin(); iter != stats.end(); ++iter) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(iter->first);
        std::cout << "Flow ID: " << iter->first << " Src Addr " << t.sourceAddress << " Dst Addr " << t.destinationAddress << std::endl;
        std::cout << "  Tx Packets: " << iter->second.txPackets << std::endl;
        std::cout << "  Rx Packets: " << iter->second.rxPackets << std::endl;
        std::cout << "  Throughput: " << iter->second.rxBytes * 8.0 / simTime / 1000 / 1000 << " Mbps" << std::endl;
        std::cout << "  Mean Delay: " << iter->second.delaySum.GetSeconds() / iter->second.rxPackets << " s" << std::endl;
    }

    Simulator::Destroy();
    return 0;
}