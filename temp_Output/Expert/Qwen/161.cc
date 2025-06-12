#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpUdpComparisonSimulation");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("TcpClient", LOG_LEVEL_INFO);

    NodeContainer nodes;
    nodes.Create(2);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices;
    devices = pointToPoint.Install(nodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    uint16_t tcpPort = 50000;
    uint16_t udpPort = 60000;

    // TCP Application
    BulkSendHelper tcpSource("ns3::TcpSocketFactory", InetSocketAddress(interfaces.GetAddress(1), tcpPort));
    tcpSource.SetAttribute("MaxBytes", UintegerValue(0));

    ApplicationContainer tcpApp = tcpSource.Install(nodes.Get(0));
    tcpApp.Start(Seconds(1.0));
    tcpApp.Stop(Seconds(10.0));

    PacketSinkHelper tcpSink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), tcpPort));
    ApplicationContainer tcpSinkApp = tcpSink.Install(nodes.Get(1));
    tcpSinkApp.Start(Seconds(1.0));
    tcpSinkApp.Stop(Seconds(10.0));

    // UDP Application
    OnOffHelper udpSource("ns3::UdpSocketFactory", InetSocketAddress(interfaces.GetAddress(1), udpPort));
    udpSource.SetConstantRate(DataRate("1Mbps"));
    udpSource.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer udpApp = udpSource.Install(nodes.Get(0));
    udpApp.Start(Seconds(1.0));
    udpApp.Stop(Seconds(10.0));

    PacketSinkHelper udpSink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), udpPort));
    ApplicationContainer udpSinkApp = udpSink.Install(nodes.Get(1));
    udpSinkApp.Start(Seconds(1.0));
    udpSinkApp.Stop(Seconds(10.0));

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    AnimationInterface anim("tcp_udp_comparison.xml");
    anim.SetConstantPosition(nodes.Get(0), 0, 0);
    anim.SetConstantPosition(nodes.Get(1), 10, 0);

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();

    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

    double tcpThroughput = 0, tcpLatency = 0, tcpPdr = 0;
    double udpThroughput = 0, udpLatency = 0, udpPdr = 0;

    for (auto it = stats.begin(); it != stats.end(); ++it) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(it->first);
        std::ostringstream protoStream;
        protoStream << (t.protocol == 6 ? "TCP" : "UDP");
        if (t.protocol == 6) { // TCP
            tcpThroughput = it->second.rxBytes * 8.0 / (it->second.timeLastRxPacket.GetSeconds() - it->second.timeFirstTxPacket.GetSeconds()) / 1000 / 1000;
            tcpLatency = it->second.delaySum.GetSeconds() / it->second.rxPackets;
            tcpPdr = static_cast<double>(it->second.rxPackets) / (it->second.txPackets);
        } else if (t.protocol == 17) { // UDP
            udpThroughput = it->second.rxBytes * 8.0 / (it->second.timeLastRxPacket.GetSeconds() - it->second.timeFirstTxPacket.GetSeconds()) / 1000 / 1000;
            udpLatency = it->second.delaySum.GetSeconds() / it->second.rxPackets;
            udpPdr = static_cast<double>(it->second.rxPackets) / (it->second.txPackets);
        }
    }

    std::cout.precision(3);
    std::cout << "TCP Throughput: " << std::fixed << tcpThroughput << " Mbps, Latency: " << tcpLatency << " s, PDR: " << tcpPdr << std::endl;
    std::cout << "UDP Throughput: " << std::fixed << udpThroughput << " Mbps, Latency: " << udpLatency << " s, PDR: " << udpPdr << std::endl;

    Simulator::Destroy();
    return 0;
}