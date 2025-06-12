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
    uint16_t sinkPort = 8000;
    double simulationTime = 10.0;

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

    // TCP Application
    Address tcpSinkAddress(InetSocketAddress(interfaces.GetAddress(1), sinkPort));
    PacketSinkHelper tcpSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
    ApplicationContainer tcpSinkApp = tcpSinkHelper.Install(nodes.Get(1));
    tcpSinkApp.Start(Seconds(0.0));
    tcpSinkApp.Stop(Seconds(simulationTime));

    OnOffHelper tcpOnOff("ns3::TcpSocketFactory", tcpSinkAddress);
    tcpOnOff.SetConstantRate(DataRate("2Mbps"));
    tcpOnOff.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer tcpClientApp = tcpOnOff.Install(nodes.Get(0));
    tcpClientApp.Start(Seconds(1.0));
    tcpClientApp.Stop(Seconds(simulationTime - 0.1));

    // UDP Application
    Address udpSinkAddress(InetSocketAddress(interfaces.GetAddress(1), sinkPort + 1));
    PacketSinkHelper udpSinkHelper("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort + 1));
    ApplicationContainer udpSinkApp = udpSinkHelper.Install(nodes.Get(1));
    udpSinkApp.Start(Seconds(0.0));
    udpSinkApp.Stop(Seconds(simulationTime));

    OnOffHelper udpOnOff("ns3::UdpSocketFactory", udpSinkAddress);
    udpOnOff.SetConstantRate(DataRate("2Mbps"));
    udpOnOff.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer udpClientApp = udpOnOff.Install(nodes.Get(0));
    udpClientApp.Start(Seconds(1.0));
    udpClientApp.Stop(Seconds(simulationTime - 0.1));

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    AnimationInterface anim("tcp_udp_comparison.xml");
    anim.SetMobilityPollInterval(Seconds(0.5));

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();
    Simulator::Destroy();

    monitor->CheckForLostPackets();

    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();
    for (auto it = stats.begin(); it != stats.end(); ++it) {
        Ipv4FlowClassifier::FiveTuple t = dynamic_cast<Ipv4FlowClassifier*>(flowmon.GetClassifier())->FindFlow(it->first);
        std::cout << "Flow ID: " << it->first << " (";
        if (t.protocol == 6) std::cout << "TCP";
        else if (t.protocol == 17) std::cout << "UDP";
        else std::cout << "Other";
        std::cout << " " << t.sourceAddress << ":" << t.sourcePort << " -> " << t.destinationAddress << ":" << t.destinationPort << ")" << std::endl;

        std::cout << "  Tx Packets: " << it->second.txPackets << std::endl;
        std::cout << "  Rx Packets: " << it->second.rxPackets << std::endl;
        std::cout << "  Lost Packets: " << it->second.lostPackets << std::endl;
        std::cout << "  Throughput: " << it->second.rxBytes * 8.0 / (it->second.timeLastRxPacket.GetSeconds() - it->second.timeFirstTxPacket.GetSeconds()) / 1000 / 1000 << " Mbps" << std::endl;
        std::cout << "  Mean Delay: " << it->second.delaySum.GetSeconds() / it->second.rxPackets << " s" << std::endl;
        std::cout << "  Packet Delivery Ratio: " << (double)it->second.rxPackets / (double)it->second.txPackets * 100.0 << " %" << std::endl;
    }

    return 0;
}