#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/aodv-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    CommandLine cmd;
    cmd.Parse(argc, argv);

    // Create nodes
    NodeContainer nodes;
    nodes.Create(4);

    // Mobility model
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::RandomDiscPositionAllocator",
                                  "X", StringValue("50.0"),
                                  "Y", StringValue("50.0"),
                                  "Z", StringValue("0.0"),
                                  "Rho", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=50.0]"));
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Mode", StringValue("Time"),
                              "Time", StringValue("1s"),
                              "Speed", StringValue("ns3::ConstantRandomVariable[Constant=1.0]"),
                              "Bounds", StringValue("50x50"));
    mobility.Install(nodes);

    // WLAN configuration
    WifiHelper wifi;
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiMacHelper mac;
    Ssid ssid = Ssid("adhoc-network");
    mac.SetType("ns3::AdhocWifiMac", "Ssid", SsidValue(ssid));

    NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

    // Install Internet stack
    AodvHelper aodv;
    InternetStackHelper stack;
    stack.SetRoutingProtocol(aodv);
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // UDP traffic
    uint16_t port = 9;
    UdpClientHelper client(interfaces.GetAddress(3), port);
    client.SetAttribute("MaxPackets", UintegerValue(1000));
    client.SetAttribute("Interval", TimeValue(MilliSeconds(10)));
    client.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = client.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(20.0));

    UdpServerHelper server(port);
    ApplicationContainer serverApps = server.Install(nodes.Get(3));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(20.0));

    // Flow monitor
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    // Animation
    AnimationInterface anim("aodv-animation.xml");
    anim.SetConstantPosition(nodes.Get(0), 10.0, 10.0);
    anim.SetConstantPosition(nodes.Get(1), 10.0, 40.0);
    anim.SetConstantPosition(nodes.Get(2), 40.0, 10.0);
    anim.SetConstantPosition(nodes.Get(3), 40.0, 40.0);

    // Run simulation
    Simulator::Stop(Seconds(20.0));
    Simulator::Run();

    // Print statistics
    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

    for (auto i = stats.begin(); i != stats.end(); ++i) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
        std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
        std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
        std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
        std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";
        std::cout << "  Packet Delivery Ratio: " << (double)i->second.rxPackets / (double)i->second.txPackets << "\n";
        std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024  << " kbps\n";
        std::cout << "  Delay Sum: " << i->second.delaySum << "\n";
        std::cout << "  Mean Delay: " << i->second.delaySum / i->second.rxPackets << "\n";

    }

    Simulator::Destroy();
    return 0;
}