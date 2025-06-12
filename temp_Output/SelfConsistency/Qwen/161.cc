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
    // Enable logging
    LogComponentEnable("TcpUdpComparisonSimulation", LOG_LEVEL_INFO);

    // Create nodes
    NodeContainer nodes;
    nodes.Create(2);

    // Setup point-to-point channel
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

    // TCP Application (BulkSend)
    uint16_t tcpPort = 50000;
    BulkSendHelper tcpSource("ns3::TcpSocketFactory", InetSocketAddress(interfaces.GetAddress(1), tcpPort));
    tcpSource.SetAttribute("MaxBytes", UintegerValue(0)); // Send continuously

    ApplicationContainer tcpApp = tcpSource.Install(nodes.Get(0));
    tcpApp.Start(Seconds(1.0));
    tcpApp.Stop(Seconds(10.0));

    PacketSinkHelper tcpSink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), tcpPort));
    ApplicationContainer tcpSinkApp = tcpSink.Install(nodes.Get(1));
    tcpSinkApp.Start(Seconds(1.0));
    tcpApp.Stop(Seconds(10.0));

    // UDP Application
    uint16_t udpPort = 60000;
    OnOffHelper udpSource("ns3::UdpSocketFactory", InetSocketAddress(interfaces.GetAddress(1), udpPort));
    udpSource.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    udpSource.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    udpSource.SetAttribute("DataRate", DataRateValue(DataRate("2Mbps")));
    udpSource.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer udpApp = udpSource.Install(nodes.Get(0));
    udpApp.Start(Seconds(1.0));
    udpApp.Stop(Seconds(10.0));

    PacketSinkHelper udpSink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), udpPort));
    ApplicationContainer udpSinkApp = udpSink.Install(nodes.Get(1));
    udpSinkApp.Start(Seconds(1.0));
    udpSinkApp.Stop(Seconds(10.0));

    // Flow Monitor setup
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    // Animation output
    AsciiTraceHelper asciiTraceHelper;
    MobilityModel mobility(nodes.Get(0)->GetObject<MobilityModel>());
    NetAnimHelper animHelper;
    animHelper.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    animHelper.EnableXml("tcp_udp_comparison.xml", 0.1);

    // Simulation run
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    // Output results
    monitor->CheckForLostPackets();
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

    double tcpThroughput = 0.0, tcpLatency = 0.0, tcpPdr = 0.0;
    double udpThroughput = 0.0, udpLatency = 0.0, udpPdr = 0.0;
    uint32_t tcpCount = 0, udpCount = 0;

    for (auto & [id, stat] : stats) {
        Ipv4FlowClassifier::FiveTuple tuple = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier())->FindFlow(id);
        if (tuple.protocol == 6) { // TCP
            tcpThroughput += stat.rxBytes * 8.0 / (stat.timeLastRxPacket.GetSeconds() - stat.timeFirstTxPacket.GetSeconds()) / 1e6;
            tcpLatency += (stat.delaySum.GetSeconds() / stat.rxPackets);
            tcpPdr += static_cast<double>(stat.rxPackets) / (stat.txPackets ? stat.txPackets : 1);
            tcpCount++;
        } else if (tuple.protocol == 17) { // UDP
            udpThroughput += stat.rxBytes * 8.0 / (stat.timeLastRxPacket.GetSeconds() - stat.timeFirstTxPacket.GetSeconds()) / 1e6;
            udpLatency += (stat.delaySum.GetSeconds() / stat.rxPackets);
            udpPdr += static_cast<double>(stat.rxPackets) / (stat.txPackets ? stat.txPackets : 1);
            udpCount++;
        }
    }

    if (tcpCount > 0) {
        tcpThroughput /= tcpCount;
        tcpLatency /= tcpCount;
        tcpPdr /= tcpCount;
    }
    if (udpCount > 0) {
        udpThroughput /= udpCount;
        udpLatency /= udpCount;
        udpPdr /= udpCount;
    }

    NS_LOG_UNCOND("=== Performance Comparison ===");
    NS_LOG_UNCOND("TCP Throughput: " << tcpThroughput << " Mbps | Latency: " << tcpLatency << " s | PDR: " << tcpPdr * 100 << "%");
    NS_LOG_UNCOND("UDP Throughput: " << udpThroughput << " Mbps | Latency: " << udpLatency << " s | PDR: " << udpPdr * 100 << "%");

    return 0;
}