#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("FqCodelTrafficControlExample");

void
TraceCwnd(uint32_t oldCwnd, uint32_t newCwnd)
{
    std::cout << Simulator::Now().GetSeconds() << "s CWND: " << newCwnd/1024 << " KB" << std::endl;
}

void
TraceRtt(Time oldRtt, Time newRtt)
{
    std::cout << Simulator::Now().GetSeconds() << "s RTT: " << newRtt.GetMilliSeconds() << " ms" << std::endl;
}

void
TraceDrop(Ptr<const QueueDiscItem> item)
{
    std::cout << Simulator::Now().GetSeconds() << "s QueueDrop: " << item->GetPacket()->GetSize() << " bytes" << std::endl;
}

void
TraceMark(Ptr<const QueueDiscItem> item)
{
    std::cout << Simulator::Now().GetSeconds() << "s QueueMark: " << item->GetPacket()->GetSize() << " bytes" << std::endl;
}

void
TraceQueueLength(uint32_t oldVal, uint32_t newVal)
{
    std::cout << Simulator::Now().GetSeconds() << "s QueueLength: " << newVal << std::endl;
}

int
main(int argc, char *argv[])
{
    bool enablePcap = true;
    bool enableDctcp = false;

    CommandLine cmd;
    cmd.AddValue("enablePcap", "Enable PCAP tracing", enablePcap);
    cmd.AddValue("enableDctcp", "Enable DCTCP (if available)", enableDctcp);
    cmd.Parse(argc, argv);

    NodeContainer nodes;
    nodes.Create(4);

    // Links:
    // n0---n1---n2---n3
    // bottleneck is link n1<->n2

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("1Gbps"));
    p2p.SetChannelAttribute("Delay", StringValue("1ms"));

    PointToPointHelper bottleneck;
    bottleneck.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    bottleneck.SetChannelAttribute("Delay", StringValue("20ms"));

    NetDeviceContainer d0d1 = p2p.Install(nodes.Get(0), nodes.Get(1));
    NetDeviceContainer d1d2 = bottleneck.Install(nodes.Get(1), nodes.Get(2));
    NetDeviceContainer d2d3 = p2p.Install(nodes.Get(2), nodes.Get(3));

    // Traffic Control: Explicit FqCoDel, dynamic queue limits
    TrafficControlHelper tch;
    uint16_t handle = tch.SetRootQueueDisc("ns3::FqCoDelQueueDisc",
                                           "Perturbation", UintegerValue(0));
    tch.SetQueueLimits("ns3::DynamicQueueLimits");
    tch.Install(d1d2);
    tch.Install(d0d1);
    tch.Install(d2d3);

    // Stack and traffic control layer
    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper ipv4;

    ipv4.SetBase("10.0.1.0", "255.255.255.0");
    Ipv4InterfaceContainer i0i1 = ipv4.Assign(d0d1);

    ipv4.SetBase("10.0.2.0", "255.255.255.0");
    Ipv4InterfaceContainer i1i2 = ipv4.Assign(d1d2);

    ipv4.SetBase("10.0.3.0", "255.255.255.0");
    Ipv4InterfaceContainer i2i3 = ipv4.Assign(d2d3);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Application setup
    uint16_t echoPort = 9;
    PingHelper ping(i2i3.GetAddress(1));
    ping.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    ping.SetAttribute("Verbose", BooleanValue(true));
    ApplicationContainer pingApps = ping.Install(nodes.Get(0));
    pingApps.Start(Seconds(0.5));
    pingApps.Stop(Seconds(10.0));

    // BulkSendHelper (TCP) from n0 to n3
    Address sinkAddress(InetSocketAddress(i2i3.GetAddress(1), 8080));
    PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", sinkAddress);
    ApplicationContainer sinkApps = sinkHelper.Install(nodes.Get(3));
    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(Seconds(12.0));

    BulkSendHelper bulkSender("ns3::TcpSocketFactory", sinkAddress);
    bulkSender.SetAttribute("MaxBytes", UintegerValue(4 * 1024 * 1024));
    ApplicationContainer bulkApps = bulkSender.Install(nodes.Get(0));
    bulkApps.Start(Seconds(1.0));
    bulkApps.Stop(Seconds(10.0));

    // TCP Trace hooks
    Ptr<Socket> ns3TcpSocket = Socket::CreateSocket(nodes.Get(0), TcpSocketFactory::GetTypeId());
    if (enableDctcp)
    {
#ifdef NS3_DCTCP
        Config::Set("/NodeList/0/$ns3::TcpL4Protocol/SocketType", TypeIdValue(TypeId::LookupByName("ns3::TcpDctcp")));
#endif
    }
    ns3TcpSocket->TraceConnectWithoutContext("CongestionWindow", MakeCallback(&TraceCwnd));
    ns3TcpSocket->TraceConnectWithoutContext("RTT", MakeCallback(&TraceRtt));

    // Connect PacketSink throughput trace
    Ptr<Application> app = sinkApps.Get(0);
    Ptr<PacketSink> pktSink = DynamicCast<PacketSink>(app);

    Simulator::Schedule(Seconds(2.0), [&pktSink] {
        double throughput = (pktSink->GetTotalRx() * 8.0) / 2e6;
        std::cout << Simulator::Now().GetSeconds() << "s Throughput: " << throughput << " Mbps" << std::endl;
    });

    // Queue Disc tracers on bottleneck (n1<->n2)
    Ptr<Node> bottleneckNode = nodes.Get(1);
    Ptr<NetDevice> bottleneckDev = d1d2.Get(0);
    Ptr<TrafficControlLayer> tcLayer = bottleneckNode->GetObject<TrafficControlLayer>();
    QueueDiscContainer qdiscs = tcLayer->GetQueueDiscsOnNetDevice(bottleneckDev);
    if (qdiscs.GetN() == 0)
        qdiscs = tch.Install(bottleneckDev); // Ensure we have a disc

    if (qdiscs.GetN() > 0)
    {
        Ptr<QueueDisc> qdisc = qdiscs.Get(0);
        qdisc->TraceConnectWithoutContext("PacketsInQueue", MakeCallback(&TraceQueueLength));
        qdisc->TraceConnectWithoutContext("Drop", MakeCallback(&TraceDrop));
        qdisc->TraceConnectWithoutContext("Mark", MakeCallback(&TraceMark));
    }

    // PCAP tracing
    if (enablePcap)
    {
        p2p.EnablePcapAll("fq_codel_tc");
        bottleneck.EnablePcapAll("fq_codel_tc_bottleneck");
    }

    Simulator::Stop(Seconds(12.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}