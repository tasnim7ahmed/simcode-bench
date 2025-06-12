#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/flow-monitor-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TrafficControlTopologyExample");

// Trace function for queue stats
void
QueueDiscStatsTrace(std::string context, uint32_t oldValue, uint32_t newValue)
{
    std::cout << Simulator::Now().GetSeconds() << "s [" << context << "] "
              << "QueueStat: " << oldValue << " -> " << newValue << std::endl;
}

// Trace function for packet drops
void
PacketDropTrace(std::string context, Ptr<const QueueDiscItem> item)
{
    NS_LOG_UNCOND("At " << Simulator::Now().GetSeconds() << "s: Packet Drop: " << context);
}

// Trace function for marked packets
void
PacketMarkTrace(std::string context, Ptr<const QueueDiscItem> item)
{
    NS_LOG_UNCOND("At " << Simulator::Now().GetSeconds() << "s: Packet Mark: " << context);
}

// Throughput calculation tracer for PacketSink
static void
CalculateThroughput(Ptr<Application> app)
{
    static uint64_t lastTotalRx = 0;
    static Time lastTime = Seconds(0);

    Ptr<PacketSink> sink = DynamicCast<PacketSink>(app);
    Time now = Simulator::Now();
    uint64_t curRx = sink->GetTotalRx();
    double throughput = (curRx - lastTotalRx) * 8.0 / 1e6 / (now.GetSeconds() - lastTime.GetSeconds() + 0.000001);

    std::cout << now.GetSeconds() << "s: App Throughput: " << throughput << " Mbps" << std::endl;

    lastTotalRx = curRx;
    lastTime = now;

    Simulator::Schedule(Seconds(1.0), &CalculateThroughput, app);
}

// RTT trace for TCP
void
RttTracer(Ptr<const TcpSocketState> state, Time oldValue, Time newValue)
{
    std::cout << Simulator::Now().GetSeconds() << "s: RTT updated, new RTT = " << newValue.GetMilliSeconds() << " ms" << std::endl;
}

// CWND trace for TCP
void
CwndTracer(Ptr<const TcpSocketState> state, uint32_t oldCwnd, uint32_t newCwnd)
{
    std::cout << Simulator::Now().GetSeconds() << "s: CWND updated, new CWND = " << newCwnd << std::endl;
}

int
main(int argc, char *argv[])
{
    bool enablePcap = true;
    bool useDctcp = false;

    CommandLine cmd;
    cmd.AddValue("enablePcap", "Enable PCAP tracing", enablePcap);
    cmd.AddValue("useDctcp", "Enable DCTCP TCP variant", useDctcp);
    cmd.Parse(argc, argv);

    // Create nodes: n0--n1--n2
    NodeContainer nodes;
    nodes.Create(3);

    InternetStackHelper stack;
    // Enable TCP ECN if DCTCP is chosen
    if (useDctcp)
    {
        Config::Set("/NodeList/*/$ns3::TcpL4Protocol/SocketType", StringValue("ns3::TcpDctcp"));
        Config::Set("/NodeList/*/$ns3::TcpL4Protocol/EcnMode", StringValue("ClassicEcn"));
    }
    stack.Install(nodes);

    // Point-to-point links
    PointToPointHelper p2p_left, p2p_right;
    // n0 <-> n1: high bandwidth, low delay
    p2p_left.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
    p2p_left.SetChannelAttribute("Delay", StringValue("1ms"));
    // n1 <-> n2: bottleneck
    p2p_right.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p_right.SetChannelAttribute("Delay", StringValue("10ms"));

    // Install devices
    NetDeviceContainer dev_left = p2p_left.Install(nodes.Get(0), nodes.Get(1));
    NetDeviceContainer dev_right = p2p_right.Install(nodes.Get(1), nodes.Get(2));

    // Traffic control: install FqCoDel on bottleneck device (n1->n2)
    TrafficControlHelper tch;
    uint16_t handle = tch.SetRootQueueDisc("ns3::FqCoDelQueueDisc");

    Config::Set("/NodeList/1/DeviceList/1/$ns3::PointToPointNetDevice/TxQueue/MaxSize", StringValue("100p")); // dynamic limit
    tch.AddInternalQueuesConfig("ns3::FqCoDelQueueDisc", "ns3::DropTailQueue", "MaxSize", StringValue("100p")); // dynamic

    tch.Install(dev_right); // Only install on bottleneck

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer iface_left = address.Assign(dev_left);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer iface_right = address.Assign(dev_right);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Install Ping Application (ICMP Echo from n0 to n2)
    uint32_t packetSize = 1024;
    uint32_t numPings = 5;
    PingHelper ping(iface_right.GetAddress(1));
    ping.SetAttribute("PacketSize", UintegerValue(packetSize));
    ping.SetAttribute("Verbose", BooleanValue(true));
    ping.SetAttribute("Count", UintegerValue(numPings));
    ApplicationContainer pingApps = ping.Install(nodes.Get(0));
    pingApps.Start(Seconds(1.0));
    pingApps.Stop(Seconds(4.0));

    // Install BulkSend Application (TCP) from n0 to n2
    uint32_t maxBytes = 10 * 1024 * 1024; // 10 MB
    BulkSendHelper bulk("ns3::TcpSocketFactory", InetSocketAddress(iface_right.GetAddress(1), 50000));
    bulk.SetAttribute("MaxBytes", UintegerValue(maxBytes));
    ApplicationContainer bulkApps = bulk.Install(nodes.Get(0));
    bulkApps.Start(Seconds(2.0));
    bulkApps.Stop(Seconds(15.0));

    // Install PacketSink on n2
    PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), 50000));
    ApplicationContainer sinkApps = sinkHelper.Install(nodes.Get(2));
    sinkApps.Start(Seconds(2.0));
    sinkApps.Stop(Seconds(16.0));

    // Trace FqCoDel queue disc on bottleneck device (n1->n2)
    Ptr<Node> bottleneckNode = nodes.Get(1);
    Ptr<NetDevice> bottleneckDevice = dev_right.Get(0); // Device on n1 towards n2
    Ptr<TrafficControlLayer> tc = bottleneckNode->GetObject<TrafficControlLayer>();
    uint32_t devId = bottleneckNode->GetDeviceIndex(bottleneckDevice);

    // Fetch the QueueDiscContainer by matching the device (based on netdevice pointer)
    QueueDiscContainer qdiscs = tc->GetQueueDiscs();
    Ptr<QueueDisc> qd;
    for (uint32_t i = 0; i < qdiscs.GetN(); ++i)
    {
        if (qdiscs.Get(i)->GetNetDevice() == bottleneckDevice)
        {
            qd = qdiscs.Get(i);
            break;
        }
    }

    if (!qd)
    {
        NS_LOG_WARN("QueueDisc not found for bottleneck device.");
    }
    else
    {
        qd->TraceConnect("PacketsInQueue", "/queue", MakeCallback(&QueueDiscStatsTrace));
        qd->TraceConnect("PacketsDropped", "/drop", MakeCallback(&PacketDropTrace));
        qd->TraceConnect("PacketsMarked", "/mark", MakeCallback(&PacketMarkTrace));
    }

    // CWND & RTT Tracing
    // Connect to the TCP socket of BulkSendApp's socket for RTT and CWND
    Ptr<BulkSendApplication> bulkApp = DynamicCast<BulkSendApplication>(bulkApps.Get(0));
    bulkApp->TraceConnectWithoutContext("Cwnd", MakeCallback(&CwndTracer));
    bulkApp->TraceConnectWithoutContext("Rtt", MakeCallback(&RttTracer));

    // Throughput tracing for sink
    Simulator::Schedule(Seconds(3.0), &CalculateThroughput, sinkApps.Get(0));

    // (Optionally) DCTCP-specific traces, if enabled
    if (useDctcp)
    {
        // For the purpose of this script, DCTCP-specific traces are not explicitly handled
    }

    // PCAP tracing
    if (enablePcap)
    {
        p2p_left.EnablePcapAll("p2p_left");
        p2p_right.EnablePcapAll("p2p_right");
    }

    // Flow monitor (optional, but fits the validation note)
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(16.0));
    Simulator::Run();

    // Print summary stats after validation
    monitor->CheckForLostPackets();
    monitor->SerializeToXmlFile("traffic-control-topology.flowmon", true, true);

    Simulator::Destroy();
    return 0;
}