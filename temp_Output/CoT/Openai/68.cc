#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/queue-disc-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/traffic-control-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ThreeNodeBottleneckTcpQueueDisc");

static void
TraceGoodput(Ptr<PacketSink> sink, std::ofstream *stream, Time interval)
{
    static uint64_t lastTotalRx = 0;
    uint64_t cur = sink->GetTotalRx();
    double goodput = (cur - lastTotalRx) * 8.0 / interval.GetSeconds() / 1e6; // Mbps
    *stream << Simulator::Now().GetSeconds() << " " << goodput << std::endl;
    lastTotalRx = cur;
    Simulator::Schedule(interval, &TraceGoodput, sink, stream, interval);
}

void
QueueLimitTracer(std::string path, uint32_t oldValue, uint32_t newValue)
{
    static std::ofstream qlimFile("queue_limits.txt", std::ios::out | std::ios::trunc);
    qlimFile << Simulator::Now().GetSeconds() << " Limit: " << newValue << std::endl;
}

void
QueueBytesTracer(uint32_t old, uint32_t cur)
{
    static std::ofstream qb("queue_bytes.txt", std::ios::out | std::ios::trunc);
    qb << Simulator::Now().GetSeconds() << " " << cur << std::endl;
}

int main(int argc, char *argv[])
{
    std::string bottleneckBandwidth = "10Mbps";
    std::string bottleneckDelay = "2ms";
    std::string queueDiscType = "FqCoDel";
    bool enableBql = false;
    double simTime = 20.0;

    CommandLine cmd;
    cmd.AddValue("bottleneckBandwidth", "Bottleneck link bandwidth", bottleneckBandwidth);
    cmd.AddValue("bottleneckDelay", "Bottleneck link delay", bottleneckDelay);
    cmd.AddValue("queueDiscType", "QueueDisc: PfifoFast, ARED, CoDel, FqCoDel or PIE", queueDiscType);
    cmd.AddValue("enableBql", "Enable Byte Queue Limits", enableBql);
    cmd.AddValue("simTime", "Simulation time (s)", simTime);
    cmd.Parse(argc, argv);

    NodeContainer nodes;
    nodes.Create(3);

    // Point-to-point links
    NodeContainer n1n2 = NodeContainer(nodes.Get(0), nodes.Get(1));
    NodeContainer n2n3 = NodeContainer(nodes.Get(1), nodes.Get(2));

    // n1-n2: 100Mbps, 0.1ms, no queue-disc
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("0.1ms"));
    p2p.SetQueue("ns3::DropTailQueue", "MaxSize", StringValue("100p"));

    NetDeviceContainer d1d2 = p2p.Install(n1n2);

    // n2-n3: adjustable, with queue disc
    PointToPointHelper p2pBn;
    p2pBn.SetDeviceAttribute("DataRate", StringValue(bottleneckBandwidth));
    p2pBn.SetChannelAttribute("Delay", StringValue(bottleneckDelay));
    if (enableBql)
    {
        p2pBn.SetQueue("ns3::BqlQueue", "MaxSize", StringValue("500p"));
    }
    else
    {
        p2pBn.SetQueue("ns3::DropTailQueue", "MaxSize", StringValue("500p"));
    }
    NetDeviceContainer d2d3 = p2pBn.Install(n2n3);

    // Install stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer i1i2 = ipv4.Assign(d1d2);

    ipv4.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer i2i3 = ipv4.Assign(d2d3);

    // Routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // QueueDisc configuration
    TrafficControlHelper tch;
    uint16_t handle = tch.SetRootQueueDisc("ns3::PfifoFastQueueDisc");
    if (queueDiscType == "PfifoFast")
    {
        tch.SetRootQueueDisc("ns3::PfifoFastQueueDisc");
    }
    else if (queueDiscType == "ARED")
    {
        tch.SetRootQueueDisc("ns3::AREDQueueDisc");
    }
    else if (queueDiscType == "CoDel")
    {
        tch.SetRootQueueDisc("ns3::CoDelQueueDisc");
    }
    else if (queueDiscType == "FqCoDel")
    {
        tch.SetRootQueueDisc("ns3::FqCoDelQueueDisc");
    }
    else if (queueDiscType == "PIE")
    {
        tch.SetRootQueueDisc("ns3::PieQueueDisc");
    }
    else
    {
        tch.SetRootQueueDisc("ns3::FqCoDelQueueDisc");
    }
    tch.SetQueueLimits("ns3::DynamicQueueLimits", "MaxPackets", UintegerValue(500));
    QueueDiscContainer qdiscs = tch.Install(d2d3.Get(0));

    // Tracing queue limits and bytes in queue
    Ptr<QueueDisc> qd = qdiscs.Get(0);
    if (qd)
    {
        qd->TraceConnectWithoutContext("Limit", MakeCallback(&QueueLimitTracer));
        if (qd->GetInternalQueue(0))
        {
            qd->GetInternalQueue(0)->TraceConnectWithoutContext("BytesInQueue", MakeCallback(&QueueBytesTracer));
        }
    }

    // TCP flows: n1->n3 and n3->n1 (bidirectional)
    uint16_t port1 = 8080;
    uint16_t port2 = 8081;

    Address n3Addr(InetSocketAddress(i2i3.GetAddress(1), port1));
    Address n1Addr(InetSocketAddress(i1i2.GetAddress(0), port2));

    // Sink on n3
    PacketSinkHelper sinkHelper1("ns3::TcpSocketFactory", n3Addr);
    ApplicationContainer sinkApp1 = sinkHelper1.Install(nodes.Get(2));
    sinkApp1.Start(Seconds(0.2));
    sinkApp1.Stop(Seconds(simTime));

    // Sink on n1
    PacketSinkHelper sinkHelper2("ns3::TcpSocketFactory", n1Addr);
    ApplicationContainer sinkApp2 = sinkHelper2.Install(nodes.Get(0));
    sinkApp2.Start(Seconds(0.2));
    sinkApp2.Stop(Seconds(simTime));

    // BulkSendApp n1->n3
    BulkSendHelper sender1("ns3::TcpSocketFactory", n3Addr);
    sender1.SetAttribute("MaxBytes", UintegerValue(0));
    ApplicationContainer senderApp1 = sender1.Install(nodes.Get(0));
    senderApp1.Start(Seconds(0.5));
    senderApp1.Stop(Seconds(simTime));

    // BulkSendApp n3->n1
    BulkSendHelper sender2("ns3::TcpSocketFactory", n1Addr);
    sender2.SetAttribute("MaxBytes", UintegerValue(0));
    ApplicationContainer senderApp2 = sender2.Install(nodes.Get(2));
    senderApp2.Start(Seconds(0.5));
    senderApp2.Stop(Seconds(simTime));

    // ICMP Ping from n1 to n3
    V4PingHelper ping(i2i3.GetAddress(1));
    ping.SetAttribute("Verbose", BooleanValue(false));
    ApplicationContainer pingApp = ping.Install(nodes.Get(0));
    pingApp.Start(Seconds(1.0));
    pingApp.Stop(Seconds(simTime));

    // Goodput trace for both directions
    Ptr<PacketSink> sink1 = DynamicCast<PacketSink>(sinkApp1.Get(0));
    Ptr<PacketSink> sink2 = DynamicCast<PacketSink>(sinkApp2.Get(0));
    std::ofstream goodput("goodput.txt", std::ios::out | std::ios::trunc);
    Simulator::Schedule(Seconds(1.0), &TraceGoodput, sink1, &goodput, Seconds(0.5));
    Simulator::Schedule(Seconds(1.0), &TraceGoodput, sink2, &goodput, Seconds(0.5));

    // FlowMonitor
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();

    // Flow statistics
    std::ofstream flowstatfile("flow_stats.txt", std::ios::out | std::ios::trunc);
    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();
    for (const auto &i : stats)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i.first);
        flowstatfile << "Flow " << i.first << " Src " << t.sourceAddress << " Dst " << t.destinationAddress
                     << " Proto " << unsigned(t.protocol)
                     << " TxPackets " << i.second.txPackets
                     << " RxPackets " << i.second.rxPackets
                     << " LostPackets " << i.second.lostPackets
                     << " TxBytes " << i.second.txBytes
                     << " RxBytes " << i.second.rxBytes
                     << " Throughput(Mbps) " << i.second.rxBytes * 8.0 / (simTime - 1.0) / 1e6
                     << " MeanDelay(s) " << (i.second.rxPackets > 0 ? i.second.delaySum.GetSeconds() / i.second.rxPackets : 0)
                     << std::endl;
    }

    Simulator::Destroy();
    goodput.close();
    flowstatfile.close();

    return 0;
}