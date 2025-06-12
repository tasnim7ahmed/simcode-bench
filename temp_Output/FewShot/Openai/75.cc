#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/queue-disc-module.h"

using namespace ns3;

void
RxTrace (std::string context, Ptr<const Packet> p, const Address &address)
{
    static std::ofstream rxTraceFile;
    static bool fileOpen = false;
    if (!fileOpen) {
        rxTraceFile.open("packet_receptions.tr", std::ios::out);
        fileOpen = true;
    }
    rxTraceFile << Simulator::Now ().GetSeconds () << " " << context << " " << p->GetSize () << std::endl;
}

void
QueueTrace(std::string context, uint32_t oldVal, uint32_t newVal)
{
    static std::ofstream queueTraceFile;
    static bool fileOpen = false;
    if (!fileOpen) {
        queueTraceFile.open("queue.tr", std::ios::out);
        fileOpen = true;
    }
    queueTraceFile << Simulator::Now().GetSeconds() << " " << context << " " << oldVal << " -> " << newVal << std::endl;
}

int main (int argc, char *argv[])
{
    NodeContainer p2pNodes01;
    p2pNodes01.Add(CreateObject<Node>()); //n0
    p2pNodes01.Add(CreateObject<Node>()); //n1
    Ptr<Node> n2 = CreateObject<Node>();  // n2 (central)
    Ptr<Node> n3 = CreateObject<Node>();
    Ptr<Node> n4 = CreateObject<Node>();
    Ptr<Node> n5 = CreateObject<Node>();
    Ptr<Node> n6 = CreateObject<Node>();

    Names::Add("n0", p2pNodes01.Get(0));
    Names::Add("n1", p2pNodes01.Get(1));
    Names::Add("n2", n2);
    Names::Add("n3", n3);
    Names::Add("n4", n4);
    Names::Add("n5", n5);
    Names::Add("n6", n6);

    // Create p2p links (n0 <-> n2), (n1 <-> n2), (n5 <-> n6)
    NodeContainer p2pNodes_n0n2(p2pNodes01.Get(0), n2);
    NodeContainer p2pNodes_n1n2(p2pNodes01.Get(1), n2);
    NodeContainer p2pNodes_n5n6(n5, n6);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices_n0n2 = pointToPoint.Install(p2pNodes_n0n2);
    NetDeviceContainer devices_n1n2 = pointToPoint.Install(p2pNodes_n1n2);

    PointToPointHelper p2p5to6;
    p2p5to6.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p5to6.SetChannelAttribute("Delay", StringValue("2ms"));
    NetDeviceContainer devices_n5n6 = p2p5to6.Install(p2pNodes_n5n6);

    // CSMA bus: n2, n3, n4, n5
    NodeContainer csmaNodes;
    csmaNodes.Add(n2);
    csmaNodes.Add(n3);
    csmaNodes.Add(n4);
    csmaNodes.Add(n5);

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay", StringValue("6560ns"));

    NetDeviceContainer csmaDevices = csma.Install(csmaNodes);

    // Internet stack
    InternetStackHelper stack;
    stack.Install(p2pNodes01);
    stack.Install(n2);
    stack.Install(n3);
    stack.Install(n4);
    stack.Install(n5);
    stack.Install(n6);

    // Assign IP addresses
    Ipv4AddressHelper address;

    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer if_n0n2 = address.Assign(devices_n0n2);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer if_n1n2 = address.Assign(devices_n1n2);

    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer if_csma = address.Assign(csmaDevices);

    address.SetBase("10.1.4.0", "255.255.255.0");
    Ipv4InterfaceContainer if_n5n6 = address.Assign(devices_n5n6);

    // Enable Global Routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Set up UDP CBR traffic from n0 to n6
    uint16_t port = 4000;
    // UDP Sink on n6
    PacketSinkHelper sinkHelper("ns3::UdpSocketFactory", InetSocketAddress(if_n5n6.GetAddress(1), port));
    ApplicationContainer sinkApp = sinkHelper.Install(n6);
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(20.0));

    // UDP OnOff (CBR) traffic on n0
    OnOffHelper onoff("ns3::UdpSocketFactory", InetSocketAddress(if_n5n6.GetAddress(1), port));
    onoff.SetAttribute("DataRate", StringValue("448kbps"));
    onoff.SetAttribute("PacketSize", UintegerValue(210));
    onoff.SetAttribute("StartTime", TimeValue(Seconds(1.0)));
    onoff.SetAttribute("StopTime", TimeValue(Seconds(20.0)));
    ApplicationContainer app = onoff.Install(p2pNodes01.Get(0));

    // Enable tracing of queues on all point-to-point and CSMA devices
    Ptr<Queue<Packet> > queue;
    for (uint32_t i = 0; i < devices_n0n2.GetN(); ++i)
    {
        queue = StaticCast<PointToPointNetDevice>(devices_n0n2.Get(i))->GetQueue();
        if (!queue.IsNull())
            queue->TraceConnect("PacketsInQueue", std::string("p2p_n0n2_dev" + std::to_string(i)), MakeCallback(&QueueTrace));
    }
    for (uint32_t i = 0; i < devices_n1n2.GetN(); ++i)
    {
        queue = StaticCast<PointToPointNetDevice>(devices_n1n2.Get(i))->GetQueue();
        if (!queue.IsNull())
            queue->TraceConnect("PacketsInQueue", std::string("p2p_n1n2_dev" + std::to_string(i)), MakeCallback(&QueueTrace));
    }
    for (uint32_t i = 0; i < devices_n5n6.GetN(); ++i)
    {
        queue = StaticCast<PointToPointNetDevice>(devices_n5n6.Get(i))->GetQueue();
        if (!queue.IsNull())
            queue->TraceConnect("PacketsInQueue", std::string("p2p_n5n6_dev" + std::to_string(i)), MakeCallback(&QueueTrace));
    }
    for (uint32_t i = 0; i < csmaDevices.GetN(); ++i)
    {
        queue = StaticCast<CsmaNetDevice>(csmaDevices.Get(i))->GetQueue();
        if (!queue.IsNull())
            queue->TraceConnect("PacketsInQueue", std::string("csma_dev" + std::to_string(i)), MakeCallback(&QueueTrace));
    }

    // Trace packet reception at sink
    sinkApp.Get(0)->TraceConnect("Rx", "udp_sink_rx", MakeCallback(&RxTrace));

    Simulator::Stop(Seconds(20.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}