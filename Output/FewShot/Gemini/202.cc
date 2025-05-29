#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable logging
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

    // Create nodes
    NodeContainer nodes;
    nodes.Create(6);

    // Configure PHY and MAC layers for 802.15.4
    NqosWifiMacHelper mac = NqosWifiMacHelper::Default();
    Wifi802154PhyHelper phy = Wifi802154PhyHelper::Default();

    WifiHelper wifi;
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                "DataMode", StringValue("OfdmRate6Mbps"),
                                "ControlMode", StringValue("OfdmRate6Mbps"));

    // Non-ADHOC mode
    Ssid ssid = Ssid("wpan-network");
    mac.SetType("ns3::NqosWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));

    NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

    // Mobility model: Circular topology
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::CirclePositionAllocator",
                                  "Radius", DoubleValue(5.0),
                                  "Theta", DoubleValue(0.0));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    // Install Internet stack
    InternetStackHelper internet;
    internet.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Sink node (data collector)
    uint16_t port = 9;
    UdpServerHelper server(port);
    ApplicationContainer sinkApp = server.Install(nodes.Get(0)); // Node 0 is the sink
    sinkApp.Start(Seconds(1.0));
    sinkApp.Stop(Seconds(10.0));

    // Client nodes (sensor nodes)
    UdpClientHelper client(interfaces.GetAddress(0), port); // Send to the sink
    client.SetAttribute("MaxPackets", UintegerValue(1000);
    client.SetAttribute("Interval", TimeValue(MilliSeconds(100)));
    client.SetAttribute("PacketSize", UintegerValue(1472));

    ApplicationContainer clientApps;
    for (uint32_t i = 1; i < nodes.GetN(); ++i) {
        clientApps.Add(client.Install(nodes.Get(i)));
    }
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Animation
    AnimationInterface anim("wpan-animation.xml");
    anim.SetMaxPktsPerTraceFile(1000000);

    // Flow monitor
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    // Run simulation
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();

    // Gather statistics
    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

    for (auto i = stats.begin(); i != stats.end(); ++i) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
        std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
        std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
        std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
        std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";
        std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024 / 1024 << " Mbps\n";
        std::cout << "  End-to-End Delay: " << i->second.delaySum.GetSeconds() / i->second.rxPackets << " s\n";
    }

    monitor->SerializeToXmlFile("wpan-flowmon.xml", true, true);

    Simulator::Destroy();
    return 0;
}