#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/dsdv-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    CommandLine cmd;
    cmd.Parse(argc, argv);

    // Number of nodes (vehicles)
    int nNodes = 10;

    // Simulation time
    double simTime = 60.0;

    // Create nodes
    NodeContainer nodes;
    nodes.Create(nNodes);

    // Mobility model: Linear movement along a straight line
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(10.0),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(nNodes),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
    mobility.Install(nodes);

    // Set initial velocities
    for (int i = 0; i < nNodes; ++i) {
        Ptr<ConstantVelocityMobilityModel> mob = nodes.Get(i)->GetObject<ConstantVelocityMobilityModel>();
        mob->SetVelocity(Vector(10, 0, 0)); // Move at 10 m/s along the x-axis
    }

    // WiFi configuration
    WifiHelper wifi;
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiMacHelper mac;
    Ssid ssid = Ssid("vanet-network");
    mac.SetType("ns3::AdhocWifiMac", "Ssid", SsidValue(ssid));

    NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

    // Install Internet stack
    DsdvHelper dsdv;
    InternetStackHelper stack;
    stack.SetRoutingProtocol("ns3::DsdvRoutingProtocol",
                             "RoutingTableSize", StringValue("65536")); //Increase routing table size to avoid routing errors
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Create applications:  Each node sends UDP packets to the last node
    uint16_t port = 9;
    UdpClientHelper client(interfaces.GetAddress(nNodes - 1), port);
    client.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    client.SetAttribute("MaxPackets", UintegerValue(100));
    client.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps;
    for (int i = 0; i < nNodes - 1; ++i) {
        clientApps.Add(client.Install(nodes.Get(i)));
    }
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(simTime));

    UdpServerHelper server(port);
    ApplicationContainer serverApp = server.Install(nodes.Get(nNodes - 1));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(simTime));

    // Flow monitor
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    // Enable tracing for packet captures (optional)
    phy.EnablePcapAll("vanet-dsdv");

    // Animation
    AnimationInterface anim("vanet-animation.xml");
    for (int i = 0; i < nNodes; ++i) {
        anim.UpdateNodeColor(nodes.Get(i), 0, 0, 255); // Blue for all nodes
    }

    // Run simulation
    Simulator::Stop(Seconds(simTime));
    Simulator::Run();

    // Print flow statistics
    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
        std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
        std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
        std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
        std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";
        std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024 / 1024 << " Mbps\n";
    }

    Simulator::Destroy();
    return 0;
}