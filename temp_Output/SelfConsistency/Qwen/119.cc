#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("AdHocWirelessNetwork");

int main(int argc, char *argv[]) {
    // Log components
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create nodes
    NodeContainer nodes;
    nodes.Create(6);

    // Setup Wi-Fi
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211a);
    wifi.SetRemoteStationManager("ns3::ArfWifiManager");

    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    phy.SetChannel(channel.Create());

    WifiMacHelper mac;
    mac.SetType("ns3::AdhocWifiMac");
    NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

    // Mobility model: grid with random variation
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(20.0),
                                  "DeltaY", DoubleValue(20.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(0, 100, 0, 100)));
    mobility.Install(nodes);

    // Install Internet stack
    InternetStackHelper internet;
    internet.Install(nodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("192.9.39.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    // Set up UDP echo servers on all nodes
    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps;
    for (uint32_t i = 0; i < nodes.GetN(); ++i) {
        serverApps.Add(echoServer.Install(nodes.Get(i)));
    }
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // Set up UDP echo clients in a ring topology
    ApplicationContainer clientApps;
    for (uint32_t i = 0; i < nodes.GetN(); ++i) {
        uint32_t nextNode = (i + 1) % nodes.GetN();
        UdpEchoClientHelper echoClient(interfaces.GetAddress(nextNode), 9);
        echoClient.SetAttribute("MaxPackets", UintegerValue(20));
        echoClient.SetAttribute("Interval", TimeValue(Seconds(0.5)));
        echoClient.SetAttribute("PacketSize", UintegerValue(1024));
        clientApps.Add(echoClient.Install(nodes.Get(i)));
    }
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Enable global routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Flow monitor setup
    FlowMonitorHelper flowmonHelper;
    Ptr<FlowMonitor> flowMonitor = flowmonHelper.InstallAll();

    // Animation output
    AnimationInterface anim("adhoc_network.xml");
    anim.EnablePacketMetadata(true);

    // Simulation run
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();

    // Output flow statistics
    flowMonitor->CheckForLostPackets();
    std::cout << "\nFlow Monitor Statistics:" << std::endl;
    std::map<FlowId, FlowMonitor::FlowStats> stats = flowMonitor->GetFlowStats();
    for (const auto &it : stats) {
        FlowId id = it.first;
        FlowMonitor::FlowStats s = it.second;
        Ipv4FlowClassifier::FiveTuple t = StaticCast<Ipv4FlowClassifier>(flowmonHelper.GetClassifier())->FindFlow(id);

        std::cout << "Flow " << id << " (" << t.sourceAddress << " -> " << t.destinationAddress << "):\n";
        std::cout << "  Tx Packets: " << s.txPackets << "\n";
        std::cout << "  Rx Packets: " << s.rxPackets << "\n";
        std::cout << "  Lost Packets: " << s.lostPackets << "\n";
        std::cout << "  Packet Delivery Ratio: " << ((double)s.rxPackets / (s.txPackets ? s.txPackets : 1)) << "\n";
        std::cout << "  Avg Delay: " << s.delaySum.ToDouble(Time::S) / (s.rxPackets ? s.rxPackets : 1) << "s\n";
        std::cout << "  Throughput: " << s.rxBytes * 8.0 / (s.timeLastRxPacket.GetSeconds() - s.timeFirstTxPacket.GetSeconds()) / 1000000 << " Mbps\n\n";
    }

    Simulator::Destroy();
    return 0;
}