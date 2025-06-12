#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable logging for applications
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
    LogComponentEnable("TcpBulkSendApplication", LOG_LEVEL_INFO);
    LogComponentEnable("PacketSink", LOG_LEVEL_INFO);

    // Set simulation time
    double simTime = 10.0;

    // Create two nodes
    NodeContainer nodes;
    nodes.Create(2);

    // Configure point-to-point link
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices = pointToPoint.Install(nodes);

    // Install internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // UDP Application: Echo Server and Client
    uint16_t udpPort = 9;
    UdpEchoServerHelper udpServer(udpPort);
    ApplicationContainer udpServerApp = udpServer.Install(nodes.Get(1));
    udpServerApp.Start(Seconds(1.0));
    udpServerApp.Stop(Seconds(simTime));

    UdpEchoClientHelper udpClient(interfaces.GetAddress(1), udpPort);
    udpClient.SetAttribute("MaxPackets", UintegerValue(1000));
    udpClient.SetAttribute("Interval", TimeValue(Seconds(0.01)));
    udpClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer udpClientApp = udpClient.Install(nodes.Get(0));
    udpClientApp.Start(Seconds(1.0));
    udpClientApp.Stop(Seconds(simTime));

    // TCP Application: BulkSend (sender) and PacketSink (receiver)
    uint16_t tcpPort = 5000;
    Address sinkAddress_tcp(InetSocketAddress(interfaces.GetAddress(1), tcpPort));
    PacketSinkHelper packetSinkTcp("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), tcpPort));
    ApplicationContainer sinkAppTcp = packetSinkTcp.Install(nodes.Get(1));
    sinkAppTcp.Start(Seconds(1.0));
    sinkAppTcp.Stop(Seconds(simTime));

    BulkSendHelper bulkSendTcp("ns3::TcpSocketFactory", sinkAddress_tcp);
    bulkSendTcp.SetAttribute("MaxBytes", UintegerValue(0)); // Send continuously

    ApplicationContainer sourceAppTcp = bulkSendTcp.Install(nodes.Get(0));
    sourceAppTcp.Start(Seconds(1.0));
    sourceAppTcp.Stop(Seconds(simTime));

    // FlowMonitor setup
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    // NetAnim visualization
    AnimationInterface anim("tcp_udp_comparison.xml");
    anim.SetConstantPosition(nodes.Get(0), 0, 0);
    anim.SetConstantPosition(nodes.Get(1), 10, 0);

    // Run simulation
    Simulator::Stop(Seconds(simTime));
    Simulator::Run();

    // Post-simulation analysis
    monitor->CheckForLostPackets();
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

    double totalTxBytesTcp = 0, totalRxBytesTcp = 0;
    double totalTxPacketsUdp = 0, totalRxPacketsUdp = 0;
    double delaySumTcp = 0, jitterSumTcp = 0, packetCountTcp = 0;
    double delaySumUdp = 0, jitterSumUdp = 0, packetCountUdp = 0;

    for (auto iter = stats.begin(); iter != stats.end(); ++iter) {
        FlowIdTag flowId;
        std::map<FlowIdTag, uint32_t>::const_iterator it = monitor->GetFlowIdToIpv4Map().find(flowId);
        if (it == monitor->GetFlowIdToIpv4Map().end()) continue;

        Ipv4FlowClassifier::FiveTuple t = DynamicCast<Ipv4FlowClassifier>(monitor->GetClassifier())->FindFlow(iter->first);
        if (t.protocol == 6) { // TCP
            totalTxBytesTcp += iter->second.txBytes;
            totalRxBytesTcp += iter->second.rxBytes;
            delaySumTcp += iter->second.delaySum.GetSeconds() * iter->second.rxPackets;
            jitterSumTcp += iter->second.jitterSum.GetSeconds() * (iter->second.rxPackets > 0 ? iter->second.rxPackets - 1 : 0);
            packetCountTcp += iter->second.rxPackets;
        } else if (t.protocol == 17) { // UDP
            totalTxPacketsUdp += iter->second.txPackets;
            totalRxPacketsUdp += iter->second.rxPackets;
            delaySumUdp += iter->second.delaySum.GetSeconds() * iter->second.rxPackets;
            jitterSumUdp += iter->second.jitterSum.GetSeconds() * (iter->second.rxPackets > 0 ? iter->second.rxPackets - 1 : 0);
            packetCountUdp += iter->second.rxPackets;
        }
    }

    // Calculate metrics
    double throughputTcp = (totalRxBytesTcp * 8) / simTime / 1e6; // Mbps
    double pdrTcp = (totalRxBytesTcp > 0 && totalTxBytesTcp > 0) ? (totalRxBytesTcp / totalTxBytesTcp) : 0;
    double avgLatencyTcp = (packetCountTcp > 0) ? (delaySumTcp / packetCountTcp) : 0;
    double avgJitterTcp = (packetCountTcp > 1) ? (jitterSumTcp / (packetCountTcp - 1)) : 0;

    double pdrUdp = (totalRxPacketsUdp > 0 && totalTxPacketsUdp > 0) ? (totalRxPacketsUdp / totalTxPacketsUdp) : 0;
    double avgLatencyUdp = (packetCountUdp > 0) ? (delaySumUdp / packetCountUdp) : 0;
    double avgJitterUdp = (packetCountUdp > 1) ? (jitterSumUdp / (packetCountUdp - 1)) : 0;

    // Output results
    std::cout << "TCP Performance Metrics:" << std::endl;
    std::cout << "Throughput: " << throughputTcp << " Mbps" << std::endl;
    std::cout << "Packet Delivery Ratio: " << pdrTcp << std::endl;
    std::cout << "Average Latency: " << avgLatencyTcp << " s" << std::endl;
    std::cout << "Average Jitter: " << avgJitterTcp << " s" << std::endl;

    std::cout << "\nUDP Performance Metrics:" << std::endl;
    std::cout << "Packet Delivery Ratio: " << pdrUdp << std::endl;
    std::cout << "Average Latency: " << avgLatencyUdp << " s" << std::endl;
    std::cout << "Average Jitter: " << avgJitterUdp << " s" << std::endl;

    Simulator::Destroy();
    return 0;
}