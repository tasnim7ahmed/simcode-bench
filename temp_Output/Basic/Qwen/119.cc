#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("AdHocWirelessNetwork");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    // Create nodes
    NodeContainer nodes;
    nodes.Create(6);

    // Setup Wi-Fi
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211a);
    wifi.SetRemoteStationManager("ns3::ArfWifiManager");

    YansWifiPhyHelper phy;
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    phy.SetChannel(channel.Create());

    WifiMacHelper mac;
    mac.SetType("ns3::AdhocWifiMac");
    NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

    // Mobility model
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(50.0),
                                  "DeltaY", DoubleValue(50.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));

    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(0, 300, 0, 300)));
    mobility.Install(nodes);

    // Install Internet Stack
    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("192.9.39.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // UDP Echo Server and Client Applications
    uint16_t port = 9;
    ApplicationContainer serverApps;
    ApplicationContainer clientApps;

    for (uint32_t i = 0; i < nodes.GetN(); ++i) {
        UdpEchoServerHelper echoServer(port);
        serverApps.Add(echoServer.Install(nodes.Get(i)));
    }

    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(20.0));

    for (uint32_t i = 0; i < nodes.GetN(); ++i) {
        uint32_t nextNode = (i + 1) % nodes.GetN();
        Ipv4Address nextAddress = interfaces.GetAddress(nextNode, 0);

        UdpEchoClientHelper echoClient(nextAddress, port);
        echoClient.SetAttribute("MaxPackets", UintegerValue(20));
        echoClient.SetAttribute("Interval", TimeValue(Seconds(0.5)));
        echoClient.SetAttribute("PacketSize", UintegerValue(512));

        clientApps.Add(echoClient.Install(nodes.Get(i)));
    }

    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(20.0));

    // Flow Monitor setup
    FlowMonitorHelper flowmonHelper;
    Ptr<FlowMonitor> flowMonitor = flowmonHelper.InstallAll();

    // Animation output
    AnimationInterface anim("adhoc_network.xml");
    anim.SetMobilityPollInterval(Seconds(0.5));

    // Simulation run
    Simulator::Stop(Seconds(20.0));
    Simulator::Run();

    // Output statistics
    flowMonitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmonHelper.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = flowMonitor->GetFlowStats();

    for (auto iter : stats) {
        FlowId id = iter.first;
        FlowMonitor::FlowStats s = iter.second;
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(id);

        std::cout << "Flow " << id << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
        std::cout << "  Tx Packets: " << s.txPackets << "\n";
        std::cout << "  Rx Packets: " << s.rxPackets << "\n";
        std::cout << "  Lost Packets: " << s.lostPackets << "\n";
        std::cout << "  Packet Delivery Ratio: " << ((double)s.rxPackets / (s.txPackets + s.lostPackets)) * 100.0 << "%\n";
        std::cout << "  Average Delay: " << s.delaySum.GetSeconds() / s.rxPackets << "s\n";
        std::cout << "  Throughput: " << s.rxBytes * 8.0 / (s.timeLastRxPacket.GetSeconds() - s.timeFirstTxPacket.GetSeconds()) / 1000.0 << " Kbps\n";
    }

    Simulator::Destroy();
    return 0;
}