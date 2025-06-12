#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/aodv-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WirelessAodvSimulation");

int main(int argc, char *argv[]) {
    uint32_t numNodes = 4;
    double totalTime = 10.0;
    bool verbose = false;
    bool enableFlowMonitor = true;
    std::string phyMode("DsssRate1Mbps");

    CommandLine cmd(__FILE__);
    cmd.AddValue("verbose", "Turn on log components", verbose);
    cmd.AddValue("numNodes", "Number of nodes for the simulation", numNodes);
    cmd.AddValue("totalTime", "Total simulation time in seconds", totalTime);
    cmd.Parse(argc, argv);

    if (verbose) {
        LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
        LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
    }

    NodeContainer nodes;
    nodes.Create(numNodes);

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());

    WifiMacHelper wifiMac;
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode", StringValue(phyMode), "ControlMode", StringValue(phyMode));

    wifiMac.SetType("ns3::AdhocWifiMac");
    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

    AodvHelper aodv;
    Ipv4ListRoutingHelper list;
    list.Add(aodv, 10);

    InternetStackHelper internet;
    internet.SetRoutingHelper(list);
    internet.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(10.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(0, 100, 0, 100)));
    mobility.Install(nodes);

    AnimationInterface anim("wireless-aodv-animation.xml");
    anim.EnablePacketMetadata(true);

    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(numNodes - 1));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(totalTime));

    UdpEchoClientHelper echoClient(interfaces.GetAddress(numNodes - 1), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(100));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(512));

    ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(totalTime));

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    Ptr<FlowMonitor> flowMonitor;
    FlowMonitorHelper flowHelper;
    if (enableFlowMonitor) {
        flowMonitor = flowHelper.InstallAll();
    }

    Simulator::Stop(Seconds(totalTime));
    Simulator::Run();

    if (enableFlowMonitor) {
        flowMonitor->CheckForLostPackets();
        FlowMonitor::FlowStatsContainer stats = flowMonitor->GetFlowStats();
        for (auto iter = stats.begin(); iter != stats.end(); ++iter) {
            Ipv4FlowClassifier::FiveTuple t = static_cast<Ipv4FlowClassifier*>(flowHelper.GetClassifier())->FindFlow(iter->first);
            std::cout << "Flow ID: " << iter->first << " Src Addr " << t.sourceAddress << " Dst Addr " << t.destinationAddress << std::endl;
            std::cout << "  Tx Packets: " << iter->second.txPackets << std::endl;
            std::cout << "  Rx Packets: " << iter->second.rxPackets << std::endl;
            std::cout << "  Throughput: " << iter->second.rxBytes * 8.0 / (iter->second.timeLastRxPacket.GetSeconds() - iter->second.timeFirstTxPacket.GetSeconds()) / 1024 / 1024 << " Mbps" << std::endl;
            std::cout << "  Mean Delay: " << iter->second.delaySum.GetSeconds() / iter->second.rxPackets << " s" << std::endl;
        }
    }

    Simulator::Destroy();
    return 0;
}