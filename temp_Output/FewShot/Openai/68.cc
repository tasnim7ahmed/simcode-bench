#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ThreeNodeBottleneckSimulation");

static void GoodputTracer(Ptr<OutputStreamWrapper> stream, uint64_t oldTotalRx, Ptr<Application> app)
{
    Ptr<PacketSink> sink = DynamicCast<PacketSink>(app);
    if (sink)
    {
        *stream->GetStream() << Simulator::Now().GetSeconds() << '\t'
                             << (sink->GetTotalRx() - oldTotalRx) * 8.0 / 1e6 /* Mbps */
                             << std::endl;
        Simulator::Schedule(Seconds(1.0), &GoodputTracer, stream, sink->GetTotalRx(), app);
    }
}

int main(int argc, char *argv[])
{
    // Defaults
    std::string bottleneckRate = "10Mbps";
    std::string bottleneckDelay = "10ms";
    std::string queueDiscType = "PfifoFastQueueDisc";
    bool enableBql = false;

    // Command line processing
    CommandLine cmd;
    cmd.AddValue("bottleneckRate", "Bandwidth of the bottleneck link (e.g., 10Mbps)", bottleneckRate);
    cmd.AddValue("bottleneckDelay", "Delay of the bottleneck link (e.g., 10ms)", bottleneckDelay);
    cmd.AddValue("queueDiscType", "QueueDisc: PfifoFastQueueDisc, AredQueueDisc, CoDelQueueDisc, FqCoDelQueueDisc, PieQueueDisc", queueDiscType);
    cmd.AddValue("enableBql", "Enable Byte Queue Limits on the bottleneck device", enableBql);
    cmd.Parse(argc, argv);

    LogComponentEnable("ThreeNodeBottleneckSimulation", LOG_LEVEL_INFO);

    // Create nodes: n1, n2, n3
    NodeContainer nodes;
    nodes.Create(3);

    // Access link: n1 <-> n2
    NodeContainer n1n2 = NodeContainer(nodes.Get(0), nodes.Get(1));
    PointToPointHelper p2pAccess;
    p2pAccess.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
    p2pAccess.SetChannelAttribute("Delay", StringValue("0.1ms"));

    // Bottleneck link: n2 <-> n3
    NodeContainer n2n3 = NodeContainer(nodes.Get(1), nodes.Get(2));
    PointToPointHelper p2pBottle;
    p2pBottle.SetDeviceAttribute("DataRate", StringValue(bottleneckRate));
    p2pBottle.SetChannelAttribute("Delay", StringValue(bottleneckDelay));

    NetDeviceContainer d1 = p2pAccess.Install(n1n2);
    NetDeviceContainer d2 = p2pBottle.Install(n2n3);

    // Enable BQL if requested (on the bottleneck devices)
    if (enableBql)
    {
        Ptr<PointToPointNetDevice> nd1 = DynamicCast<PointToPointNetDevice>(d2.Get(0));
        Ptr<PointToPointNetDevice> nd2 = DynamicCast<PointToPointNetDevice>(d2.Get(1));
        nd1->GetQueue()->SetAttribute("MaxBytes", UintegerValue(262144)); // Example value
        nd2->GetQueue()->SetAttribute("MaxBytes", UintegerValue(262144));
    }

    // Install Internet Stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.0.1.0", "255.255.255.0");
    Ipv4InterfaceContainer i1 = address.Assign(d1);

    address.SetBase("10.0.2.0", "255.255.255.0");
    Ipv4InterfaceContainer i2 = address.Assign(d2);

    // Set up queue disc on the bottleneck (n2->n3 direction and n3->n2, both)
    TrafficControlHelper tch;
    if (queueDiscType == "PfifoFastQueueDisc")
    {
        tch.SetRootQueueDisc("ns3::PfifoFastQueueDisc");
    }
    else if (queueDiscType == "AredQueueDisc")
    {
        tch.SetRootQueueDisc("ns3::AredQueueDisc");
    }
    else if (queueDiscType == "CoDelQueueDisc")
    {
        tch.SetRootQueueDisc("ns3::CoDelQueueDisc");
    }
    else if (queueDiscType == "FqCoDelQueueDisc")
    {
        tch.SetRootQueueDisc("ns3::FqCoDelQueueDisc");
    }
    else if (queueDiscType == "PieQueueDisc")
    {
        tch.SetRootQueueDisc("ns3::PieQueueDisc");
    }
    else
    {
        NS_ABORT_MSG("Invalid queueDiscType");
    }
    // Set appropriate queue disc attributes if needed.

    QueueDiscContainer qdis = tch.Install(d2);

    // Tracing queue length, queue limit
    AsciiTraceHelper ascii;
    std::string qlimitFile = "queue-limit.tr";
    std::string qlenFile = "queue-bytes.tr";
    Ptr<OutputStreamWrapper> qlimitStream = ascii.CreateFileStream(qlimitFile);
    Ptr<OutputStreamWrapper> qlenStream = ascii.CreateFileStream(qlenFile);

    if (qdis.GetN() > 0)
    {
        Ptr<QueueDisc> qd = qdis.Get(0);
        qd->TraceConnectWithoutContext("BytesInQueue", MakeBoundCallback(
            [](Ptr<OutputStreamWrapper> stream, uint32_t oldValue, uint32_t newValue){
                *stream->GetStream() << Simulator::Now().GetSeconds() 
                                    << '\t' << newValue << std::endl;
            }, qlenStream));
        qd->TraceConnectWithoutContext("Limit", MakeBoundCallback(
            [](Ptr<OutputStreamWrapper> stream, uint32_t oldValue, uint32_t newValue){
                *stream->GetStream() << Simulator::Now().GetSeconds() 
                                    << '\t' << newValue << std::endl;
            }, qlimitStream));
    }

    // TCP Flows: n1 -> n3 and n3 -> n1 (bidirectional)
    uint16_t tcpPort = 5000;
    // Sink on n3, source on n1
    Address sinkAddress1(InetSocketAddress(i2.GetAddress(1), tcpPort));
    PacketSinkHelper sinkHelper1("ns3::TcpSocketFactory", sinkAddress1);
    ApplicationContainer sinkApp1 = sinkHelper1.Install(nodes.Get(2));
    sinkApp1.Start(Seconds(0.0));
    sinkApp1.Stop(Seconds(21.0));

    OnOffHelper client1("ns3::TcpSocketFactory", sinkAddress1);
    client1.SetAttribute("DataRate", StringValue("50Mbps"));
    client1.SetAttribute("PacketSize", UintegerValue(1448));
    client1.SetAttribute("StartTime", TimeValue(Seconds(1.0)));
    client1.SetAttribute("StopTime", TimeValue(Seconds(21.0)));
    client1.SetAttribute("MaxBytes", UintegerValue(0)); // Unlimited
    ApplicationContainer clientApp1 = client1.Install(nodes.Get(0));

    // Sink on n1, source on n3
    Address sinkAddress2(InetSocketAddress(i1.GetAddress(0), tcpPort));
    PacketSinkHelper sinkHelper2("ns3::TcpSocketFactory", sinkAddress2);
    ApplicationContainer sinkApp2 = sinkHelper2.Install(nodes.Get(0));
    sinkApp2.Start(Seconds(0.0));
    sinkApp2.Stop(Seconds(21.0));

    OnOffHelper client2("ns3::TcpSocketFactory", sinkAddress2);
    client2.SetAttribute("DataRate", StringValue("50Mbps"));
    client2.SetAttribute("PacketSize", UintegerValue(1448));
    client2.SetAttribute("StartTime", TimeValue(Seconds(1.0)));
    client2.SetAttribute("StopTime", TimeValue(Seconds(21.0)));
    client2.SetAttribute("MaxBytes", UintegerValue(0));
    ApplicationContainer clientApp2 = client2.Install(nodes.Get(2));

    // Goodput tracing
    Ptr<PacketSink> sink1 = DynamicCast<PacketSink>(sinkApp1.Get(0));
    Ptr<PacketSink> sink2 = DynamicCast<PacketSink>(sinkApp2.Get(0));
    Ptr<OutputStreamWrapper> goodputStream = ascii.CreateFileStream("goodput.tr");
    Simulator::Schedule(Seconds(2.0), &GoodputTracer, goodputStream, 0, sinkApp1.Get(0));
    Simulator::Schedule(Seconds(2.0), &GoodputTracer, goodputStream, 0, sinkApp2.Get(0));

    // Ping: n1 -> n3
    V4PingHelper ping(i2.GetAddress(1));
    ping.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    ping.SetAttribute("Verbose", BooleanValue(false));
    ApplicationContainer pingApps = ping.Install(nodes.Get(0));
    pingApps.Start(Seconds(0.1));
    pingApps.Stop(Seconds(21.0));

    // Enable FlowMonitor for all nodes
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    // Simulation run
    Simulator::Stop(Seconds(22.0));
    Simulator::Run();

    // Output flow monitor statistics
    monitor->SerializeToXmlFile("flowmon.xml", true, true);

    // Optionally, text summary for flow statistics
    std::ofstream flowStatFile;
    flowStatFile.open("flow-stats.tr", std::ios::out);

    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();
    for (auto &flow : stats)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flow.first);
        flowStatFile << "Flow " << flow.first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
        flowStatFile << "  Tx Packets: " << flow.second.txPackets << "\n";
        flowStatFile << "  Rx Packets: " << flow.second.rxPackets << "\n";
        flowStatFile << "  Tx Bytes:   " << flow.second.txBytes << "\n";
        flowStatFile << "  Rx Bytes:   " << flow.second.rxBytes << "\n";
        flowStatFile << "  Duration:   " << (flow.second.timeLastRxPacket.GetSeconds() - flow.second.timeFirstTxPacket.GetSeconds()) << " s\n";
        flowStatFile << "  Throughput: " << flow.second.rxBytes * 8.0 / (flow.second.timeLastRxPacket.GetSeconds()-flow.second.timeFirstTxPacket.GetSeconds()) / 1e6 << " Mbps\n";
    }
    flowStatFile.close();

    Simulator::Destroy();
    return 0;
}