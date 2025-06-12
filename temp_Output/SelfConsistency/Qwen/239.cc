#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpP2PNetwork");

int main(int argc, char *argv[]) {
    // Enable logging for TCP-related components
    LogComponentEnable("TcpL4Protocol", LOG_LEVEL_INFO);
    LogComponentEnable("TcpSocketBase", LOG_LEVEL_INFO);

    // Create nodes: client, router, and server
    NodeContainer nodes;
    nodes.Create(3);

    NodeContainer clientRouter(nodes.Get(0), nodes.Get(1));
    NodeContainer routerServer(nodes.Get(1), nodes.Get(2));

    // Set up Point-to-Point links
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer clientRouterDevices = p2p.Install(clientRouter);
    NetDeviceContainer routerServerDevices = p2p.Install(routerServer);

    // Install Internet stack on all nodes
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;

    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer clientRouterInterfaces = address.Assign(clientRouterDevices);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer routerServerInterfaces = address.Assign(routerServerDevices);

    // Set up routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Set up applications
    uint16_t sinkPort = 8080;

    // Server (sink) application
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
    ApplicationContainer serverApp = packetSinkHelper.Install(nodes.Get(2)); // install on server node
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Client application
    OnOffHelper clientHelper("ns3::TcpSocketFactory", InetSocketAddress(routerServerInterfaces.GetAddress(1), sinkPort));
    clientHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    clientHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    clientHelper.SetAttribute("DataRate", DataRateValue(DataRate("1Mbps")));
    clientHelper.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp = clientHelper.Install(nodes.Get(0)); // install on client node
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Enable flow monitor
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    // Simulation setup
    Simulator::Stop(Seconds(11.0));
    Simulator::Run();

    // Output flow statistics
    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator iter = stats.begin(); iter != stats.end(); ++iter) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(iter->first);
        std::cout << "Flow ID: " << iter->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
        std::cout << "  Tx Packets: " << iter->second.txPackets << "\n";
        std::cout << "  Rx Packets: " << iter->second.rxPackets << "\n";
        std::cout << "  Lost Packets: " << iter->second.lostPackets << "\n";
        std::cout << "  Throughput: " << (iter->second.rxBytes * 8.0) / (iter->second.timeLastRxPacket.GetSeconds() - iter->second.timeFirstTxPacket.GetSeconds()) / 1000000 << " Mbps\n";
    }

    Simulator::Destroy();
    return 0;
}