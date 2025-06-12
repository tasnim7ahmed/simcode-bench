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

    NodeContainer nodes;
    nodes.Create(2);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices = pointToPoint.Install(nodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    uint16_t tcpPort = 50000;
    uint16_t udpPort = 50001;
    double simTime = 10.0;

    // TCP Application
    uint32_t tcpPacketSize = 1024;
    uint32_t tcpMaxBytes = 0; // unlimited
    ApplicationContainer tcpSinkApp;
    ApplicationContainer tcpSourceApp;

    // TCP Sink on node 1
    Address tcpLocalAddress(InetSocketAddress(Ipv4Address::GetAny(), tcpPort));
    PacketSinkHelper tcpSinkHelper("ns3::TcpSocketFactory", tcpLocalAddress);
    tcpSinkApp = tcpSinkHelper.Install(nodes.Get(1));
    tcpSinkApp.Start(Seconds(0.0));
    tcpSinkApp.Stop(Seconds(simTime));

    // TCP BulkSend on node 0
    BulkSendHelper tcpSource("ns3::TcpSocketFactory", InetSocketAddress(interfaces.GetAddress(1), tcpPort));
    tcpSource.SetAttribute("MaxBytes", UintegerValue(tcpMaxBytes));
    tcpSource.SetAttribute("SendSize", UintegerValue(tcpPacketSize));
    tcpSourceApp = tcpSource.Install(nodes.Get(0));
    tcpSourceApp.Start(Seconds(0.1));
    tcpSourceApp.Stop(Seconds(simTime - 0.1));

    // UDP Application
    uint32_t udpPacketSize = 1024;
    uint32_t udpMaxPackets = 4294967295U;
    double udpInterval = 0.001; // 1ms interval = 1kpps

    ApplicationContainer udpSinkApp;
    ApplicationContainer udpSourceApp;

    // UDP Sink on node 1
    PacketSinkHelper udpSinkHelper("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), udpPort));
    udpSinkApp = udpSinkHelper.Install(nodes.Get(1));
    udpSinkApp.Start(Seconds(0.0));
    udpSinkApp.Stop(Seconds(simTime));

    // UDP Client on node 0
    UdpClientHelper udpClient(interfaces.GetAddress(1), udpPort);
    udpClient.SetAttribute("MaxPackets", UintegerValue(udpMaxPackets));
    udpClient.SetAttribute("Interval", TimeValue(Seconds(udpInterval)));
    udpClient.SetAttribute("PacketSize", UintegerValue(udpPacketSize));
    udpSourceApp = udpClient.Install(nodes.Get(0));
    udpSourceApp.Start(Seconds(0.1));
    udpSourceApp.Stop(Seconds(simTime - 0.1));

    // Mobility (for NetAnim, though static)
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);
    Ptr<MobilityModel> mob0 = nodes.Get(0)->GetObject<MobilityModel>();
    Ptr<MobilityModel> mob1 = nodes.Get(1)->GetObject<MobilityModel>();
    mob0->SetPosition(Vector(10, 30, 0));
    mob1->SetPosition(Vector(60, 30, 0));

    AnimationInterface anim("tcp_udp_comparison.xml");
    anim.SetConstantPosition(nodes.Get(0), 10, 30);
    anim.SetConstantPosition(nodes.Get(1), 60, 30);

    FlowMonitorHelper flowmonHelper;
    Ptr<FlowMonitor> flowmon = flowmonHelper.InstallAll();

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();

    flowmon->CheckForLostPackets();

    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmonHelper.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = flowmon->GetFlowStats();

    double tcpThroughput = 0, udpThroughput = 0;
    double tcpDelaySum = 0, udpDelaySum = 0;
    uint32_t tcpRxPackets = 0, udpRxPackets = 0;
    uint32_t tcpTxPackets = 0, udpTxPackets = 0;
    uint32_t tcpLostPackets = 0, udpLostPackets = 0;

    for (const auto &flow : stats)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flow.first);
        bool isTcp = (t.protocol == 6);
        bool isUdp = (t.protocol == 17);

        double rxBytes = flow.second.rxBytes * 8.0 / (simTime - 0.2) / 1000000.0;
        double meanDelay = (flow.second.rxPackets > 0) ? (flow.second.delaySum.GetSeconds() / flow.second.rxPackets) : 0.0;
        double pdr = (flow.second.txPackets > 0) ? (double)flow.second.rxPackets / flow.second.txPackets : 0.0;

        if (isTcp)
        {
            tcpThroughput += rxBytes;
            tcpDelaySum += flow.second.delaySum.GetSeconds();
            tcpRxPackets += flow.second.rxPackets;
            tcpTxPackets += flow.second.txPackets;
            tcpLostPackets += flow.second.lostPackets;
        }
        else if (isUdp)
        {
            udpThroughput += rxBytes;
            udpDelaySum += flow.second.delaySum.GetSeconds();
            udpRxPackets += flow.second.rxPackets;
            udpTxPackets += flow.second.txPackets;
            udpLostPackets += flow.second.lostPackets;
        }
    }

    std::cout << "=== TCP vs UDP Performance (10s Simulation) ===" << std::endl;
    std::cout << "TCP Throughput: " << tcpThroughput << " Mbps" << std::endl;
    std::cout << "UDP Throughput: " << udpThroughput << " Mbps" << std::endl;

    std::cout << "TCP Avg Latency: "
              << (tcpRxPackets ? tcpDelaySum / tcpRxPackets * 1e3 : 0) << " ms" << std::endl;
    std::cout << "UDP Avg Latency: "
              << (udpRxPackets ? udpDelaySum / udpRxPackets * 1e3 : 0) << " ms" << std::endl;

    std::cout << "TCP Packet Delivery Ratio: "
              << (tcpTxPackets ? (double)tcpRxPackets / tcpTxPackets : 0) << std::endl;
    std::cout << "UDP Packet Delivery Ratio: "
              << (udpTxPackets ? (double)udpRxPackets / udpTxPackets : 0) << std::endl;

    std::cout << "TCP Lost Packets: " << tcpLostPackets << std::endl;
    std::cout << "UDP Lost Packets: " << udpLostPackets << std::endl;

    Simulator::Destroy();
    return 0;
}