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
    LogComponentEnable("UdpClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("TcpClientApplication", LOG_LEVEL_INFO);

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

    uint16_t port = 9;

    // TCP Server
    Address tcpServerAddress(InetSocketAddress(interfaces.GetAddress(1), port));
    PacketSinkHelper tcpSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer tcpSinkApp = tcpSinkHelper.Install(nodes.Get(1));
    tcpSinkApp.Start(Seconds(0.0));
    tcpSinkApp.Stop(Seconds(10.0));

    // TCP Client
    OnOffHelper tcpOnOff("ns3::TcpSocketFactory", tcpServerAddress);
    tcpOnOff.SetConstantDataRate(DataRate("1Mbps"));
    tcpOnOff.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer tcpClientApp = tcpOnOff.Install(nodes.Get(0));
    tcpClientApp.Start(Seconds(1.0));
    tcpClientApp.Stop(Seconds(9.0));

    // UDP Server
    PacketSinkHelper udpSinkHelper("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port + 1));
    ApplicationContainer udpSinkApp = udpSinkHelper.Install(nodes.Get(1));
    udpSinkApp.Start(Seconds(0.0));
    udpSinkApp.Stop(Seconds(10.0));

    // UDP Client
    Address udpServerAddress(InetSocketAddress(interfaces.GetAddress(1), port + 1));
    OnOffHelper udpOnOff("ns3::UdpSocketFactory", udpServerAddress);
    udpOnOff.SetConstantDataRate(DataRate("1Mbps"));
    udpOnOff.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer udpClientApp = udpOnOff.Install(nodes.Get(0));
    udpClientApp.Start(Seconds(1.0));
    udpClientApp.Stop(Seconds(9.0));

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    AnimationInterface anim("tcp_udp_comparison.xml");
    anim.SetMobilityPollInterval(Seconds(1));

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();

    monitor->CheckForLostPackets();
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

    for (auto & [flowId, flowStats] : stats) {
        Ipv4FlowClassifier::FiveTuple t = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier())->FindFlow(flowId);
        std::cout << "Flow ID: " << flowId << " Protocol: " << (t.protocol == 6 ? "TCP" : "UDP") << std::endl;
        std::cout << "  Tx Packets: " << flowStats.txPackets << std::endl;
        std::cout << "  Rx Packets: " << flowStats.rxPackets << std::endl;
        std::cout << "  Lost Packets: " << flowStats.lostPackets << std::endl;
        std::cout << "  Throughput: " << flowStats.rxBytes * 8.0 / (flowStats.timeLastRxPacket.GetSeconds() - flowStats.timeFirstTxPacket.GetSeconds()) / 1000 / 1000 << " Mbps" << std::endl;
        std::cout << "  Average Delay: " << flowStats.delaySum.GetSeconds() / flowStats.rxPackets << " s" << std::endl;
        std::cout << "  Packet Delivery Ratio: " << (double)flowStats.rxPackets / (flowStats.txPackets ? flowStats.txPackets : 1) << std::endl;
    }

    Simulator::Destroy();
    return 0;
}