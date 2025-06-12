#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/flow-monitor-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("NetworkSimulation");

int main(int argc, char *argv[]) {
    std::string queueDiscType = "PfifoFast";
    bool enableBql = false;
    uint32_t bottleneckBandwidthKbps = 1000;
    Time bottleneckDelay = MilliSeconds(10);
    double simulationTime = 10.0;

    CommandLine cmd(__FILE__);
    cmd.AddValue("queueDiscType", "Queue disc type to use on the bottleneck link (PfifoFast, ARED, CoDel, FqCoDel, PIE)", queueDiscType);
    cmd.AddValue("enableBql", "Enable Byte Queue Limits (BQL) on the bottleneck link", enableBql);
    cmd.AddValue("bottleneckBandwidthKbps", "Bottleneck bandwidth in Kbps", bottleneckBandwidthKbps);
    cmd.AddValue("bottleneckDelay", "Bottleneck delay in ms", bottleneckDelay);
    cmd.AddValue("simulationTime", "Total simulation time in seconds", simulationTime);
    cmd.Parse(argc, argv);

    bottleneckDelay = MilliSeconds(bottleneckDelay.GetMilliSeconds());

    NodeContainer nodes;
    nodes.Create(3);

    PointToPointHelper p2pAccess;
    p2pAccess.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
    p2pAccess.SetChannelAttribute("Delay", StringValue("0.1ms"));

    PointToPointHelper p2pBottleneck;
    p2pBottleneck.SetDeviceAttribute("DataRate", DataRateValue(DataRate(std::to_string(bottleneckBandwidthKbps) + "kbps")));
    p2pBottleneck.SetChannelAttribute("Delay", TimeValue(bottleneckDelay));

    InternetStackHelper stack;
    stack.Install(nodes);

    NetDeviceContainer devAccess1 = p2pAccess.Install(nodes.Get(0), nodes.Get(1));
    NetDeviceContainer devAccess2 = p2pBottleneck.Install(nodes.Get(1), nodes.Get(2));

    TrafficControlHelper tchBottleneck;
    if (queueDiscType == "PfifoFast") {
        tchBottleneck.SetRootQueueDisc("ns3::PfifoFastQueueDisc");
    } else if (queueDiscType == "ARED") {
        tchBottleneck.SetRootQueueDisc("ns3::AredQueueDisc");
    } else if (queueDiscType == "CoDel") {
        tchBottleneck.SetRootQueueDisc("ns3::CoDelQueueDisc");
    } else if (queueDiscType == "FqCoDel") {
        tchBottleneck.SetRootQueueDisc("ns3::FqCoDelQueueDisc");
    } else if (queueDiscType == "PIE") {
        tchBottleneck.SetRootQueueDisc("ns3::PieQueueDisc");
    }

    if (enableBql) {
        tchBottleneck.SetQueueLimits("ns3::DynamicQueueLimits");
    }

    tchBottleneck.Install(devAccess2);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces1 = ipv4.Assign(devAccess1);
    ipv4.NewNetwork();
    Ipv4InterfaceContainer interfaces2 = ipv4.Assign(devAccess2);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    uint16_t port = 5000;

    // Bidirectional TCP flows
    BulkSendHelper source1("ns3::TcpSocketFactory", InetSocketAddress(interfaces2.GetAddress(1), port));
    source1.SetAttribute("MaxBytes", UintegerValue(0));
    ApplicationContainer appSource1 = source1.Install(nodes.Get(0));
    appSource1.Start(Seconds(1.0));
    appSource1.Stop(Seconds(simulationTime - 1.0));

    PacketSinkHelper sink1("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer appSink1 = sink1.Install(nodes.Get(2));
    appSink1.Start(Seconds(0.0));
    appSink1.Stop(Seconds(simulationTime));

    BulkSendHelper source2("ns3::TcpSocketFactory", InetSocketAddress(interfaces1.GetAddress(0), port + 1));
    source2.SetAttribute("MaxBytes", UintegerValue(0));
    ApplicationContainer appSource2 = source2.Install(nodes.Get(2));
    appSource2.Start(Seconds(1.0));
    appSource2.Stop(Seconds(simulationTime - 1.0));

    PacketSinkHelper sink2("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port + 1));
    ApplicationContainer appSink2 = sink2.Install(nodes.Get(0));
    appSink2.Start(Seconds(0.0));
    appSink2.Stop(Seconds(simulationTime));

    // Ping from n1 to n3
    V4PingHelper ping(interfaces2.GetAddress(1));
    ping.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    ping.SetAttribute("Size", UintegerValue(56));
    ApplicationContainer pingApp = ping.Install(nodes.Get(0));
    pingApp.Start(Seconds(0.0));
    pingApp.Stop(Seconds(simulationTime));

    // Flow monitor
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    AsciiTraceHelper ascii;
    p2pBottleneck.EnableAsciiAll(ascii.CreateFileStream("queue-disc-bottleneck.tr"));

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    monitor->CheckForLostPackets();
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
    for (auto iter = stats.begin(); iter != stats.end(); ++iter) {
        Ipv4FlowClassifier::FiveTuple t = static_cast<Ipv4FlowClassifier*>(monitor->GetClassifier().Peek())->FindFlow(iter->first);
        if (t.sourcePort == port || t.sourcePort == port + 1) {
            std::cout << "Flow ID: " << iter->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")" << std::endl;
            std::cout << "  Tx Bytes: " << iter->second.txBytes << std::endl;
            std::cout << "  Rx Bytes: " << iter->second.rxBytes << std::endl;
            std::cout << "  Goodput: " << (iter->second.rxBytes * 8.0 / (iter->second.timeLastRxPacket.GetSeconds() - iter->second.timeFirstTxPacket.GetSeconds()) / 1000000.0) << " Mbps" << std::endl;
        }
    }

    Simulator::Destroy();
    return 0;
}