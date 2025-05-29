// main.cc
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/queue-disc.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ThreeNodeBottleneck");

enum QueueDiscType
{
    PFIFO_FAST,
    ARED,
    CODEL,
    FQ_CODEL,
    PIE
};

std::string QueueDiscTypeToString(QueueDiscType type)
{
    switch (type)
    {
    case PFIFO_FAST:
        return "PfifoFast";
    case ARED:
        return "Ared";
    case CODEL:
        return "CoDel";
    case FQ_CODEL:
        return "FqCoDel";
    case PIE:
        return "Pie";
    default:
        return "Unknown";
    }
}

void
TraceQueueLimits(std::string context, uint32_t oldValue, uint32_t newValue)
{
    std::cout << Simulator::Now().GetSeconds() << " [QueueLimits] " << context
              << " old: " << oldValue << " new: " << newValue << std::endl;
}

void
TraceQueueBytes(std::string context, uint32_t qSize)
{
    std::cout << Simulator::Now().GetSeconds() << " [QueueBytes] " << context
              << " QueueBytes: " << qSize << std::endl;
}

void
TraceGoodput(Ptr<Socket> socket, uint64_t &lastTotalRx, double interval)
{
    Address addr;
    Ptr<Ipv4> ipv4 = socket->GetNode()->GetObject<Ipv4>();
    if (ipv4)
    {
        addr = InetSocketAddress(ipv4->GetAddress(1, 0).GetLocal(), 0);
    }
    uint64_t totalRx = socket->GetNode()->GetObject<Ipv4L3Protocol> ()->GetRxBytes();
    double goodput = (totalRx - lastTotalRx) * 8.0 / (interval * 1e6); // Mbps
    std::cout << Simulator::Now().GetSeconds()
              << " [Goodput] Node " << socket->GetNode()->GetId()
              << " Goodput: " << goodput << " Mbps" << std::endl;
    lastTotalRx = totalRx;
    Simulator::Schedule(Seconds(interval), &TraceGoodput, socket, std::ref(lastTotalRx), interval);
}

void
PingRttCallback(std::string context, Time rtt)
{
    std::cout << Simulator::Now().GetSeconds()
              << " [PingRTT] " << context
              << " RTT: " << rtt.GetMilliSeconds() << " ms" << std::endl;
}

int
main(int argc, char *argv[])
{
    // Default parameter values
    std::string bottleneckBandwidth = "10Mbps";
    std::string bottleneckDelay = "10ms";
    QueueDiscType queueDiscType = PIE;
    bool enableBql = false;
    uint32_t nFlows = 2;
    double simTime = 15.0;

    // Parse command-line arguments
    CommandLine cmd(__FILE__);
    cmd.AddValue("bottleneckBandwidth", "Bandwidth of the bottleneck link", bottleneckBandwidth);
    cmd.AddValue("bottleneckDelay", "Delay of the bottleneck link", bottleneckDelay);
    cmd.AddValue("queueDiscType", "Queue disc type: PfifoFast|Ared|CoDel|FqCoDel|Pie", (int &)queueDiscType);
    cmd.AddValue("enableBql", "Enable Byte Queue Limits (BQL) on bottleneck link", enableBql);
    cmd.AddValue("nFlows", "Number of TCP bidirectional flows", nFlows);
    cmd.AddValue("simTime", "Simulation time in seconds", simTime);
    cmd.Parse(argc, argv);

    // Create nodes
    NodeContainer nodes;
    nodes.Create(3); // n1, n2, n3

    NodeContainer n1n2 = NodeContainer(nodes.Get(0), nodes.Get(1));
    NodeContainer n2n3 = NodeContainer(nodes.Get(1), nodes.Get(2));

    // Point-to-point links
    PointToPointHelper p2pAccess;
    p2pAccess.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
    p2pAccess.SetChannelAttribute("Delay", StringValue("0.1ms"));

    NetDeviceContainer devices1 = p2pAccess.Install(n1n2);

    PointToPointHelper p2pBottleneck;
    p2pBottleneck.SetDeviceAttribute("DataRate", StringValue(bottleneckBandwidth));
    p2pBottleneck.SetChannelAttribute("Delay", StringValue(bottleneckDelay));

    if (enableBql)
    {
        p2pBottleneck.SetQueue("ns3::ByteQueue", "MaxBytes", UintegerValue(10000));
    }
    else
    {
        p2pBottleneck.SetQueue("ns3::DropTailQueue", "MaxPackets", UintegerValue(1000));
    }

    NetDeviceContainer devices2 = p2pBottleneck.Install(n2n3);

    // Install TCP/IP stack
    InternetStackHelper internet;
    internet.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer i1i2 = ipv4.Assign(devices1);

    ipv4.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer i2i3 = ipv4.Assign(devices2);

    // Set up Traffic Control (queue disc) on the bottleneck link (n2->n3 side)
    TrafficControlHelper tch;
    uint16_t handle = tch.SetRootQueueDisc(
        (queueDiscType == PFIFO_FAST) ? "ns3::PfifoFastQueueDisc" :
        (queueDiscType == ARED)       ? "ns3::AredQueueDisc" :
        (queueDiscType == CODEL)      ? "ns3::CoDelQueueDisc" :
        (queueDiscType == FQ_CODEL)   ? "ns3::FqCoDelQueueDisc" :
                                        "ns3::PieQueueDisc");

    QueueDiscContainer qdiscs = tch.Install(devices2.Get(0)); // At n2 device facing n3

    // Monitor QueueDisc: trace queue limits and queue bytes
    if (qdiscs.GetN() > 0)
    {
        qdiscs.Get(0)->TraceConnect("BytesInQueue", std::string("bottleneck-queue"), MakeCallback(&TraceQueueBytes));
#if NS_MAJOR_VERSION >= 3 && NS_MINOR_VERSION >= 36
        // If ns-3 supports queue disc limit tracing
        qdiscs.Get(0)->TraceConnect("Limit", std::string("bottleneck-queue-limit"), MakeCallback(&TraceQueueLimits));
#endif
    }

    // Enable BQL/queue traces at the NetDevice layer if desired
    if (enableBql)
    {
        Ptr<PointToPointNetDevice> dev = DynamicCast<PointToPointNetDevice>(devices2.Get(0));
        if (dev)
        {
            Ptr<Queue<Packet>> queue = dev->GetQueue();
            if (queue)
            {
                queue->TraceConnectWithoutContext("BytesInQueue", MakeCallback(&TraceQueueBytes));
#if NS_MAJOR_VERSION >= 3 && NS_MINOR_VERSION >= 36
                queue->TraceConnectWithoutContext("Limit", MakeCallback(&TraceQueueLimits));
#endif
            }
        }
    }

    // Set TCP variant
    Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpNewReno::GetTypeId()));

    // Applications: Set up bidirectional TCP flows
    uint16_t portBase = 5000;

    ApplicationContainer serverApps, clientApps;

    for (uint32_t i = 0; i < nFlows; ++i)
    {
        // n1 -> n3
        Address sinkLocalAddress1(InetSocketAddress(i2i3.GetAddress(1), portBase + i));
        PacketSinkHelper sinkHelper1("ns3::TcpSocketFactory", sinkLocalAddress1);
        serverApps.Add(sinkHelper1.Install(nodes.Get(2)));

        OnOffHelper client1("ns3::TcpSocketFactory", InetSocketAddress(i2i3.GetAddress(1), portBase + i));
        client1.SetAttribute("DataRate", DataRateValue(DataRate("20Mbps")));
        client1.SetAttribute("PacketSize", UintegerValue(1000));
        client1.SetAttribute("StartTime", TimeValue(Seconds(1)));
        client1.SetAttribute("StopTime", TimeValue(Seconds(simTime)));
        clientApps.Add(client1.Install(nodes.Get(0)));

        // n3 -> n1
        Address sinkLocalAddress2(InetSocketAddress(i1i2.GetAddress(0), portBase + nFlows + i));
        PacketSinkHelper sinkHelper2("ns3::TcpSocketFactory", sinkLocalAddress2);
        serverApps.Add(sinkHelper2.Install(nodes.Get(0)));

        OnOffHelper client2("ns3::TcpSocketFactory", InetSocketAddress(i1i2.GetAddress(0), portBase + nFlows + i));
        client2.SetAttribute("DataRate", DataRateValue(DataRate("20Mbps")));
        client2.SetAttribute("PacketSize", UintegerValue(1000));
        client2.SetAttribute("StartTime", TimeValue(Seconds(1)));
        client2.SetAttribute("StopTime", TimeValue(Seconds(simTime)));
        clientApps.Add(client2.Install(nodes.Get(2)));
    }

    // Ping (ICMP Echo) from n1 to n3 to measure RTT
    V4PingHelper ping(i2i3.GetAddress(1));
    ping.SetAttribute("Verbose", BooleanValue(true));
    ApplicationContainer pingApps = ping.Install(nodes.Get(0));
    pingApps.Start(Seconds(0.5));
    pingApps.Stop(Seconds(simTime));
    Config::Connect("/NodeList/0/ApplicationList/*/$ns3::V4Ping/Rtt", MakeCallback(&PingRttCallback));

    // FlowMonitor
    FlowMonitorHelper flowHelper;
    Ptr<FlowMonitor> flowMonitor = flowHelper.InstallAll();

    // Goodput measurement: monitor PacketSink at n3 (for n1->n3) and n1 (for n3->n1)
    uint64_t lastTotalRx_n3 = 0, lastTotalRx_n1 = 0;
    Ptr<Application> app_n3_sink = serverApps.Get(0);
    Ptr<Application> app_n1_sink = serverApps.Get(1);
    Ptr<PacketSink> sink_n3 = DynamicCast<PacketSink>(app_n3_sink);
    Ptr<PacketSink> sink_n1 = DynamicCast<PacketSink>(app_n1_sink);

    Simulator::Schedule(Seconds(1.0), [&sink_n3, &lastTotalRx_n3]() {
        Simulator::Schedule(Seconds(1.0), &TraceGoodput, sink_n3->GetSocket(), std::ref(lastTotalRx_n3), 1.0);
    });
    Simulator::Schedule(Seconds(1.0), [&sink_n1, &lastTotalRx_n1]() {
        Simulator::Schedule(Seconds(1.0), &TraceGoodput, sink_n1->GetSocket(), std::ref(lastTotalRx_n1), 1.0);
    });

    // Enable PCAP tracing
    p2pAccess.EnablePcapAll("three-node-p2p");
    p2pBottleneck.EnablePcapAll("three-node-bottleneck");

    Simulator::Stop(Seconds(simTime + 1));
    Simulator::Run();

    // Output flow monitor statistics
    flowMonitor->SerializeToXmlFile("flow-monitor-results.xml", true, true);

    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowHelper.GetClassifier());
    FlowMonitor::FlowStatsContainer stats = flowMonitor->GetFlowStats();

    std::ofstream flowStatFile("flow-statistics.txt");
    for (const auto &flow : stats)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flow.first);
        flowStatFile << "Flow " << flow.first << " : " << t.sourceAddress << " --> "
                     << t.destinationAddress << "\n";
        flowStatFile << "  Tx Bytes: " << flow.second.txBytes << "\n";
        flowStatFile << "  Rx Bytes: " << flow.second.rxBytes << "\n";
        flowStatFile << "  Throughput: "
                     << flow.second.rxBytes * 8.0 / (simTime * 1e6)
                     << " Mbps\n";
        flowStatFile << "  Mean Delay: "
                     << (flow.second.delaySum.GetSeconds() / flow.second.rxPackets)
                     << " s\n";
    }
    flowStatFile.close();

    Simulator::Destroy();
    return 0;
}