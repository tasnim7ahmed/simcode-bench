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

    // Create nodes
    NodeContainer nodes;
    nodes.Create(2);

    // Setup point-to-point link
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("5ms"));

    NetDeviceContainer devices;
    devices = pointToPoint.Install(nodes);

    // Install Internet Stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Install TCP Application (On node 0 -> node 1)
    uint16_t tcpPort = 50000;
    Address tcpSinkAddress(InetSocketAddress(interfaces.GetAddress(1), tcpPort));
    PacketSinkHelper tcpSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), tcpPort));
    ApplicationContainer tcpSinkApp = tcpSinkHelper.Install(nodes.Get(1));
    tcpSinkApp.Start(Seconds(0.0));
    tcpSinkApp.Stop(Seconds(10.0));

    OnOffHelper tcpOnOff("ns3::TcpSocketFactory", tcpSinkAddress);
    tcpOnOff.SetAttribute("DataRate", DataRateValue(DataRate("2Mbps")));
    tcpOnOff.SetAttribute("PacketSize", UintegerValue(1024));
    tcpOnOff.SetAttribute("StartTime", TimeValue(Seconds(1.0)));
    tcpOnOff.SetAttribute("StopTime", TimeValue(Seconds(9.0)));
    ApplicationContainer tcpApp = tcpOnOff.Install(nodes.Get(0));

    // Install UDP Application (On node 0 -> node 1)
    uint16_t udpPort = 40000;
    Address udpSinkAddress(InetSocketAddress(interfaces.GetAddress(1), udpPort));
    PacketSinkHelper udpSinkHelper("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), udpPort));
    ApplicationContainer udpSinkApp = udpSinkHelper.Install(nodes.Get(1));
    udpSinkApp.Start(Seconds(0.0));
    udpSinkApp.Stop(Seconds(10.0));

    OnOffHelper udpOnOff("ns3::UdpSocketFactory", udpSinkAddress);
    udpOnOff.SetAttribute("DataRate", DataRateValue(DataRate("2Mbps")));
    udpOnOff.SetAttribute("PacketSize", UintegerValue(1024));
    udpOnOff.SetAttribute("StartTime", TimeValue(Seconds(1.0)));
    udpOnOff.SetAttribute("StopTime", TimeValue(Seconds(9.0)));
    ApplicationContainer udpApp = udpOnOff.Install(nodes.Get(0));

    // Setup FlowMonitor
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    // Enable NetAnim
    AnimationInterface anim("tcp_vs_udp.xml");
    anim.SetConstantPosition(nodes.Get(0), 10, 20);
    anim.SetConstantPosition(nodes.Get(1), 60, 20);

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();

    // Calculate metrics for TCP and UDP
    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

    double tcpThroughput = 0.0, udpThroughput = 0.0;
    double tcpDelay = 0.0, udpDelay = 0.0;
    uint32_t tcpRxPackets = 0, udpRxPackets = 0;
    uint32_t tcpTxPackets = 0, udpTxPackets = 0;
    uint32_t tcpLostPackets = 0, udpLostPackets = 0;

    for (const auto& flow : stats) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flow.first);
        bool isTcp = (t.destinationPort == tcpPort);
        bool isUdp = (t.destinationPort == udpPort);

        double throughput = flow.second.rxBytes * 8.0 / (9.0 - 1.0) / 1000 / 1000; // Mb/s (from 1s to 9s)
        double meanDelay = (flow.second.rxPackets > 0) ? (flow.second.delaySum.GetSeconds() / flow.second.rxPackets) : 0.0;

        if (isTcp) {
            tcpThroughput += throughput;
            tcpDelay += meanDelay;
            tcpRxPackets += flow.second.rxPackets;
            tcpTxPackets += flow.second.txPackets;
            tcpLostPackets += flow.second.lostPackets;
        }
        if (isUdp) {
            udpThroughput += throughput;
            udpDelay += meanDelay;
            udpRxPackets += flow.second.rxPackets;
            udpTxPackets += flow.second.txPackets;
            udpLostPackets += flow.second.lostPackets;
        }
    }

    double tcpPDR = (tcpTxPackets > 0) ? (double)tcpRxPackets / tcpTxPackets * 100.0 : 0.0;
    double udpPDR = (udpTxPackets > 0) ? (double)udpRxPackets / udpTxPackets * 100.0 : 0.0;
    double tcpMeanDelayMs = (tcpRxPackets > 0) ? tcpDelay * 1000.0 : 0.0;
    double udpMeanDelayMs = (udpRxPackets > 0) ? udpDelay * 1000.0 : 0.0;

    std::cout << "==== Performance Comparison ====" << std::endl;
    std::cout << "Protocol\tThroughput(Mbps)\tLatency(ms)\tPDR(%)" << std::endl;
    std::cout << "TCP\t\t" << tcpThroughput << "\t\t"
              << tcpMeanDelayMs << "\t\t"
              << tcpPDR << std::endl;
    std::cout << "UDP\t\t" << udpThroughput << "\t\t"
              << udpMeanDelayMs << "\t\t"
              << udpPDR << std::endl;

    Simulator::Destroy();
    return 0;
}