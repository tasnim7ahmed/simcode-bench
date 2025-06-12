#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("P2PTCPNetworkSimulation");

int main(int argc, char *argv[]) {
    // Simulation configuration
    std::string tcpProtocol = "TcpNewReno";
    Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpNewReno::GetTypeId()));
    uint16_t serverPort = 50000;
    double simulationTime = 10.0;

    // Create nodes
    NodeContainer nodes;
    nodes.Create(3);

    // Create point-to-point links
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices0_1 = p2p.Install(nodes.Get(0), nodes.Get(1));
    NetDeviceContainer devices1_2 = p2p.Install(nodes.Get(1), nodes.Get(2));

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces0_1 = address.Assign(devices0_1);
    address.NewNetwork();
    Ipv4InterfaceContainer interfaces1_2 = address.Assign(devices1_2);

    // Set up routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Set up TCP server on Node 2
    PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), serverPort));
    ApplicationContainer serverApps = sink.Install(nodes.Get(2));
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(simulationTime));

    // Set up TCP client on Node 0
    OnOffHelper client("ns3::TcpSocketFactory", InetSocketAddress(interfaces1_2.GetAddress(1), serverPort));
    client.SetAttribute("DataRate", DataRateValue(DataRate("2Mbps")));
    client.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps = client.Install(nodes.Get(0));
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(simulationTime - 0.1));

    // Enable PCAP tracing
    p2p.EnablePcapAll("p2p-tcp-network");

    // Setup FlowMonitor
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    // Run simulation
    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    // Print FlowMonitor results
    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
        if (t.sourceAddress == interfaces0_1.GetAddress(0) && t.destinationAddress == interfaces1_2.GetAddress(1)) {
            std::cout << "\nFlow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << "):\n";
            std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
            std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
            std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";
            std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / simulationTime / 1000 / 1000 << " Mbps\n";
        }
    }

    Simulator::Destroy();
    return 0;
}