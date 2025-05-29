#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // Enable logging for demonstration
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
    LogComponentEnable("BulkSendApplication", LOG_LEVEL_INFO);

    // Create two nodes
    NodeContainer nodes;
    nodes.Create(2);

    // Configure point-to-point channel
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices;
    devices = pointToPoint.Install(nodes);

    // Install the internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Set up UDP application: UDP Server on node 1, UDP Client on node 0
    uint16_t udpPort = 8000;
    UdpServerHelper udpServer (udpPort);
    ApplicationContainer udpServerApps = udpServer.Install (nodes.Get(1));
    udpServerApps.Start (Seconds(0.0));
    udpServerApps.Stop (Seconds(10.0));

    UdpClientHelper udpClient (interfaces.GetAddress(1), udpPort);
    udpClient.SetAttribute("MaxPackets", UintegerValue(10000));
    udpClient.SetAttribute("Interval", TimeValue(MilliSeconds(10)));
    udpClient.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer udpClientApps = udpClient.Install (nodes.Get(0));
    udpClientApps.Start (Seconds(1.0));
    udpClientApps.Stop (Seconds(10.0));

    // Set up TCP application: BulkSendApplication on node 0, PacketSink on node 1
    uint16_t tcpPort = 9000;
    Address sinkAddress (InetSocketAddress (interfaces.GetAddress(1), tcpPort));
    PacketSinkHelper tcpSinkHelper ("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), tcpPort));
    ApplicationContainer tcpSinkApps = tcpSinkHelper.Install (nodes.Get(1));
    tcpSinkApps.Start (Seconds(0.0));
    tcpSinkApps.Stop (Seconds(10.0));

    BulkSendHelper tcpSource ("ns3::TcpSocketFactory", sinkAddress);
    tcpSource.SetAttribute("MaxBytes", UintegerValue(0)); // Unlimited data
    tcpSource.SetAttribute("SendSize", UintegerValue(1024));
    ApplicationContainer tcpSourceApps = tcpSource.Install (nodes.Get(0));
    tcpSourceApps.Start (Seconds(1.0));
    tcpSourceApps.Stop (Seconds(10.0));

    // Animation: Set node positions for NetAnim
    AnimationInterface anim("tcp-udp-comparison.xml");
    anim.SetConstantPosition(nodes.Get(0), 20.0, 30.0);
    anim.SetConstantPosition(nodes.Get(1), 80.0, 30.0);

    // Set up FlowMonitor
    Ptr<FlowMonitor> flowMonitor;
    FlowMonitorHelper flowHelper;
    flowMonitor = flowHelper.InstallAll();

    // Run simulation
    Simulator::Stop(Seconds(10.1));
    Simulator::Run();

    // Gather and print FlowMonitor statistics
    flowMonitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowHelper.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = flowMonitor->GetFlowStats();

    uint64_t tcpTxBytes = 0, tcpRxBytes = 0, tcpTxPackets = 0, tcpRxPackets = 0, tcpLostPackets = 0;
    double tcpDelaySum = 0.0;
    uint64_t udpTxBytes = 0, udpRxBytes = 0, udpTxPackets = 0, udpRxPackets = 0, udpLostPackets = 0;
    double udpDelaySum = 0.0;
    uint32_t tcpFlows = 0, udpFlows = 0;

    for (const auto& flow : stats)
    {
        Ipv4FlowClassifier::FiveTuple tuple = classifier->FindFlow(flow.first);
        if (tuple.destinationPort == tcpPort)
        {
            ++tcpFlows;
            tcpTxBytes += flow.second.txBytes;
            tcpRxBytes += flow.second.rxBytes;
            tcpTxPackets += flow.second.txPackets;
            tcpRxPackets += flow.second.rxPackets;
            tcpLostPackets += flow.second.lostPackets;
            tcpDelaySum += flow.second.delaySum.GetSeconds();
        }
        else if (tuple.destinationPort == udpPort)
        {
            ++udpFlows;
            udpTxBytes += flow.second.txBytes;
            udpRxBytes += flow.second.rxBytes;
            udpTxPackets += flow.second.txPackets;
            udpRxPackets += flow.second.rxPackets;
            udpLostPackets += flow.second.lostPackets;
            udpDelaySum += flow.second.delaySum.GetSeconds();
        }
    }

    double simulationTime = 9.0; // Data applications start at 1.0s, end at 10s

    std::cout << "===== Performance Comparison TCP vs UDP =====" << std::endl;

    std::cout << "\n--- TCP ---" << std::endl;
    std::cout << "Throughput: " << (tcpRxBytes * 8.0 / simulationTime) / 1e6 << " Mbps" << std::endl;
    double avgTcpDelay = tcpRxPackets ? (tcpDelaySum / tcpRxPackets) : 0.0;
    std::cout << "Average Latency: " << avgTcpDelay * 1000 << " ms" << std::endl;
    double tcpPdr = tcpTxPackets ? (static_cast<double>(tcpRxPackets) / tcpTxPackets) * 100 : 0;
    std::cout << "Packet Delivery Ratio: " << tcpPdr << " %" << std::endl;

    std::cout << "\n--- UDP ---" << std::endl;
    std::cout << "Throughput: " << (udpRxBytes * 8.0 / simulationTime) / 1e6 << " Mbps" << std::endl;
    double avgUdpDelay = udpRxPackets ? (udpDelaySum / udpRxPackets) : 0.0;
    std::cout << "Average Latency: " << avgUdpDelay * 1000 << " ms" << std::endl;
    double udpPdr = udpTxPackets ? (static_cast<double>(udpRxPackets) / udpTxPackets) * 100 : 0;
    std::cout << "Packet Delivery Ratio: " << udpPdr << " %" << std::endl;

    Simulator::Destroy();
    return 0;
}