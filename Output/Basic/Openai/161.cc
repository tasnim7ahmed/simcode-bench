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
    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
    LogComponentEnable("BulkSendApplication", LOG_LEVEL_INFO);
    LogComponentEnable("PacketSink", LOG_LEVEL_INFO);

    uint32_t packetSize = 1024;
    uint32_t maxPacketCount = 10000;
    double simTime = 10.0;

    NodeContainer nodes;
    nodes.Create(2);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices;
    devices = pointToPoint.Install(nodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // TCP Applications
    uint16_t tcpPort = 50000;
    Address tcpSinkAddress(InetSocketAddress(interfaces.GetAddress(1), tcpPort));

    PacketSinkHelper tcpSinkHelper("ns3::TcpSocketFactory", tcpSinkAddress);
    ApplicationContainer tcpSinkApp = tcpSinkHelper.Install(nodes.Get(1));
    tcpSinkApp.Start(Seconds(0.0));
    tcpSinkApp.Stop(Seconds(simTime));

    BulkSendHelper tcpSourceHelper("ns3::TcpSocketFactory", tcpSinkAddress);
    tcpSourceHelper.SetAttribute("MaxBytes", UintegerValue(0));
    ApplicationContainer tcpSourceApp = tcpSourceHelper.Install(nodes.Get(0));
    tcpSourceApp.Start(Seconds(1.0));
    tcpSourceApp.Stop(Seconds(simTime));

    // UDP Applications
    uint16_t udpPort = 40000;
    UdpServerHelper udpServer(udpPort);
    ApplicationContainer udpServerApp = udpServer.Install(nodes.Get(1));
    udpServerApp.Start(Seconds(0.0));
    udpServerApp.Stop(Seconds(simTime));

    UdpClientHelper udpClient(interfaces.GetAddress(1), udpPort);
    udpClient.SetAttribute("MaxPackets", UintegerValue(maxPacketCount));
    udpClient.SetAttribute("Interval", TimeValue(Seconds(0.001))); // 1ms
    udpClient.SetAttribute("PacketSize", UintegerValue(packetSize));
    ApplicationContainer udpClientApp = udpClient.Install(nodes.Get(0));
    udpClientApp.Start(Seconds(1.0));
    udpClientApp.Stop(Seconds(simTime));

    // NetAnim
    AnimationInterface anim("tcp-udp-compare.xml");
    anim.SetConstantPosition(nodes.Get(0), 0.0, 50.0);
    anim.SetConstantPosition(nodes.Get(1), 100.0, 50.0);

    // FlowMonitor
    FlowMonitorHelper flowmonHelper;
    Ptr<FlowMonitor> monitor = flowmonHelper.InstallAll();

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();

    monitor->CheckForLostPackets();

    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmonHelper.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

    double tcpThroughput = 0.0, udpThroughput = 0.0;
    double tcpDelaySum = 0.0, udpDelaySum = 0.0;
    uint32_t tcpRxPackets = 0, tcpTxPackets = 0, tcpRxBytes = 0;
    uint32_t udpRxPackets = 0, udpTxPackets = 0, udpRxBytes = 0;
    uint32_t tcpFlows = 0, udpFlows = 0, tcpLost = 0, udpLost = 0;

    for (auto& flow : stats)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flow.first);
        if (t.destinationPort == tcpPort)
        {
            tcpRxBytes += flow.second.rxBytes;
            tcpRxPackets += flow.second.rxPackets;
            tcpTxPackets += flow.second.txPackets;
            tcpLost += flow.second.lostPackets;
            tcpDelaySum += flow.second.delaySum.GetSeconds();
            tcpThroughput += ((flow.second.rxBytes * 8.0) / (simTime - 1.0)) / 1e6;
            tcpFlows++;
        }
        else if (t.destinationPort == udpPort)
        {
            udpRxBytes += flow.second.rxBytes;
            udpRxPackets += flow.second.rxPackets;
            udpTxPackets += flow.second.txPackets;
            udpLost += flow.second.lostPackets;
            udpDelaySum += flow.second.delaySum.GetSeconds();
            udpThroughput += ((flow.second.rxBytes * 8.0) / (simTime - 1.0)) / 1e6;
            udpFlows++;
        }
    }

    std::cout << "===== TCP vs UDP Performance =====" << std::endl;

    std::cout << "\n--- TCP ---" << std::endl;
    std::cout << "Throughput: " << tcpThroughput << " Mbps" << std::endl;
    std::cout << "Average Latency: ";
    if (tcpRxPackets > 0)
        std::cout << (tcpDelaySum / tcpRxPackets) * 1000.0 << " ms" << std::endl;
    else
        std::cout << "N/A" << std::endl;
    std::cout << "Packet Delivery Ratio: ";
    if (tcpTxPackets > 0)
        std::cout << (double)tcpRxPackets / tcpTxPackets * 100.0 << " %" << std::endl;
    else
        std::cout << "N/A" << std::endl;

    std::cout << "\n--- UDP ---" << std::endl;
    std::cout << "Throughput: " << udpThroughput << " Mbps" << std::endl;
    std::cout << "Average Latency: ";
    if (udpRxPackets > 0)
        std::cout << (udpDelaySum / udpRxPackets) * 1000.0 << " ms" << std::endl;
    else
        std::cout << "N/A" << std::endl;
    std::cout << "Packet Delivery Ratio: ";
    if (udpTxPackets > 0)
        std::cout << (double)udpRxPackets / udpTxPackets * 100.0 << " %" << std::endl;
    else
        std::cout << "N/A" << std::endl;

    Simulator::Destroy();
    return 0;
}