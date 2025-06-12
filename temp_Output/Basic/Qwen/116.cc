#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("AdHocWirelessNetwork");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    // Create nodes
    NodeContainer nodes;
    nodes.Create(2);

    // Setup wireless channel and phy
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());

    // Setup MAC layer for ad hoc mode
    WifiMacHelper mac;
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211a);
    wifi.SetRemoteStationManager("ns3::ArfWifiManager");

    mac.SetType("ns3::AdhocWifiMac");
    NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

    // Mobility setup
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(2),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(0, 100, 0, 100)));
    mobility.Install(nodes);

    // Install Internet stack
    InternetStackHelper internet;
    internet.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("192.9.39.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    // Setup UDP echo server on both nodes (port 9)
    UdpEchoServerHelper echoServer(9);

    ApplicationContainer serverApps1 = echoServer.Install(nodes.Get(0));
    serverApps1.Start(Seconds(1.0));
    serverApps1.Stop(Seconds(10.0));

    ApplicationContainer serverApps2 = echoServer.Install(nodes.Get(1));
    serverApps2.Start(Seconds(1.0));
    serverApps2.Stop(Seconds(10.0));

    // Setup UDP echo client on both nodes to send to each other
    UdpEchoClientHelper echoClient1(interfaces.GetAddress(1), 9);
    echoClient1.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient1.SetAttribute("Interval", TimeValue(Seconds(1.)));
    echoClient1.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps1 = echoClient1.Install(nodes.Get(0));
    clientApps1.Start(Seconds(2.0));
    clientApps1.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient2(interfaces.GetAddress(0), 9);
    echoClient2.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient2.SetAttribute("Interval", TimeValue(Seconds(1.)));
    echoClient2.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps2 = echoClient2.Install(nodes.Get(1));
    clientApps2.Start(Seconds(2.0));
    clientApps2.Stop(Seconds(10.0));

    // Flow monitoring setup
    FlowMonitorHelper flowmonHelper;
    Ptr<FlowMonitor> flowMonitor = flowmonHelper.InstallAll();

    // Enable animation
    AnimationInterface anim("adhoc_network.xml");
    anim.SetMobilityPollInterval(Seconds(0.5));

    // Simulation run
    Simulator::Stop(Seconds(11.0));
    Simulator::Run();

    // Output flow statistics
    flowMonitor->CheckForLostPackets();
    FlowMonitor::FlowStatsContainer stats = flowMonitor->GetFlowStats();

    std::cout << "\nFlow Statistics:\n";
    std::cout << "--------------------------------------------------\n";
    std::cout << "| Src Addr \t| Dst Addr \t| Pkt Sent | Pkt Recv | Lost | Delay (ms) | Throughput |\n";
    std::cout << "--------------------------------------------------\n";

    for (auto itr = stats.begin(); itr != stats.end(); ++itr) {
        Ipv4FlowClassifier::FiveTuple t = dynamic_cast<Ipv4FlowClassifier*>(flowmonHelper.GetClassifier())->FindFlow(itr->first);
        std::ostringstream srcAddr, dstAddr;
        srcAddr << t.sourceAddress << ":" << t.sourcePort;
        dstAddr << t.destinationAddress << ":" << t.destinationPort;

        double delayMs = (itr->second.delaySum.GetSeconds() / itr->second.rxPackets) * 1000;
        double throughput = (itr->second.rxBytes * 8.0) / (itr->second.timeLastRxPacket.GetSeconds() - itr->second.timeFirstTxPacket.GetSeconds()) / 1000; // kbps

        std::cout << "| " << srcAddr.str() << "\t| " << dstAddr.str() << "\t| "
                  << itr->second.txPackets << "\t| " << itr->second.rxPackets << "\t| "
                  << itr->second.lostPackets << "\t| " << delayMs << "\t| " << throughput << " kbps |\n";
    }

    std::cout << "--------------------------------------------------\n";

    Simulator::Destroy();
    return 0;
}