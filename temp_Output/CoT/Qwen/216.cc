#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("P2PTCPNetworkSimulation");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create nodes
    NodeContainer nodes;
    nodes.Create(3);

    // Create point-to-point links
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // Connect node 0 to node 1
    NetDeviceContainer devices0_1 = p2p.Install(nodes.Get(0), nodes.Get(1));

    // Connect node 1 to node 2
    NetDeviceContainer devices1_2 = p2p.Install(nodes.Get(1), nodes.Get(2));

    // Install Internet Stack
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

    // Set up TCP server on node 2
    uint16_t sinkPort = 8080;
    Address sinkAddress(InetSocketAddress(interfaces1_2.GetAddress(1), sinkPort));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
    ApplicationContainer sinkApps = packetSinkHelper.Install(nodes.Get(2));
    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(Seconds(10.0));

    // Set up TCP client on node 0
    BulkSendHelper bulkSend("ns3::TcpSocketFactory", sinkAddress);
    bulkSend.SetAttribute("MaxBytes", UintegerValue(0)); // Send continuously
    ApplicationContainer clientApps = bulkSend.Install(nodes.Get(0));
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(10.0));

    // Enable PCAP tracing
    p2p.EnablePcapAll("p2p_tcp_network");

    // Setup Flow Monitor
    FlowMonitorHelper flowmonHelper;
    Ptr<FlowMonitor> flowMonitor = flowmonHelper.InstallAll();

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();

    // Print performance statistics
    flowMonitor->CheckForLostPackets();
    std::cout << "\nNetwork Performance Statistics:" << std::endl;
    std::map<FlowId, FlowMonitor::FlowStats> stats = flowMonitor->GetFlowStats();
    for (auto it = stats.begin(); it != stats.end(); ++it) {
        FlowId flowId = it->first;
        FlowMonitor::FlowStats flow = it->second;

        double throughput = (flow.rxBytes * 8.0) / (flow.timeLastRxPacket.GetSeconds() - flow.timeFirstTxPacket.GetSeconds()) / 1000; // in kbps
        uint64_t packetLoss = flow.lostPackets;

        std::cout << "Flow ID: " << flowId << std::endl;
        std::cout << "  Throughput: " << throughput << " Kbps" << std::endl;
        std::cout << "  Packet Loss: " << packetLoss << " packets" << std::endl;
    }

    Simulator::Destroy();
    return 0;
}