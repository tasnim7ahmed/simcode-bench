#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/queue-disc-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("MixedPointToPointCsmaRouting");

void
InterfaceDown(Ptr<Node> node, uint32_t ifIndex)
{
    Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
    ipv4->SetDown(ifIndex);
}

void
InterfaceUp(Ptr<Node> node, uint32_t ifIndex)
{
    Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
    ipv4->SetUp(ifIndex);
}

void
RecomputeRoutes()
{
    Ipv4GlobalRoutingHelper::RecomputeRoutingTables();
}

void
QueueTrace(std::string context, uint32_t oldVal, uint32_t newVal)
{
    std::ofstream out("queue-trace.txt", std::ios_base::app);
    out << Simulator::Now().GetSeconds() << " " << context << " " << oldVal << " " << newVal << std::endl;
}

void
PacketSinkTrace(Ptr<const Packet> packet, const Address &address)
{
    static std::ofstream out("packet-recv.txt", std::ios_base::app);
    out << Simulator::Now().GetSeconds() << " " << packet->GetSize() << std::endl;
}

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);
    LogComponentEnable("MixedPointToPointCsmaRouting", LOG_LEVEL_INFO);

    NodeContainer nodes;
    nodes.Create(7);

    // Point-to-Point Links
    NodeContainer n0n1 = NodeContainer(nodes.Get(0), nodes.Get(1));
    NodeContainer n2n3 = NodeContainer(nodes.Get(2), nodes.Get(3));
    NodeContainer n1n4 = NodeContainer(nodes.Get(1), nodes.Get(4));
    NodeContainer n4n5 = NodeContainer(nodes.Get(4), nodes.Get(5));
    NodeContainer n5n6 = NodeContainer(nodes.Get(5), nodes.Get(6));
    NodeContainer n2n4 = NodeContainer(nodes.Get(2), nodes.Get(4));
    NodeContainer n3n6 = NodeContainer(nodes.Get(3), nodes.Get(6));

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // Install point-to-point
    NetDeviceContainer d0d1 = p2p.Install(n0n1); // Used for alternative path
    NetDeviceContainer d1d4 = p2p.Install(n1n4); // primary path
    NetDeviceContainer d4d5 = p2p.Install(n4n5);
    NetDeviceContainer d5d6 = p2p.Install(n5n6);
    NetDeviceContainer d2d3 = p2p.Install(n2n3);
    NetDeviceContainer d2d4 = p2p.Install(n2n4);
    NetDeviceContainer d3d6 = p2p.Install(n3n6);

    // CSMA/CD segment: nodes 1,2,3
    NodeContainer csmaNodes = NodeContainer(nodes.Get(1), nodes.Get(2), nodes.Get(3));
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));
    NetDeviceContainer csmaDevices = csma.Install(csmaNodes);

    InternetStackHelper stack;
    Ipv4GlobalRoutingHelper globalRouting;
    stack.SetRoutingHelper(globalRouting);
    stack.Install(nodes);

    // Address assignment
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer i0i1 = address.Assign(d0d1);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer i1i4 = address.Assign(d1d4);

    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer i4i5 = address.Assign(d4d5);

    address.SetBase("10.1.4.0", "255.255.255.0");
    Ipv4InterfaceContainer i5i6 = address.Assign(d5d6);

    address.SetBase("10.1.5.0", "255.255.255.0");
    Ipv4InterfaceContainer i2i3 = address.Assign(d2d3);

    address.SetBase("10.1.6.0", "255.255.255.0");
    Ipv4InterfaceContainer i2i4 = address.Assign(d2d4);

    address.SetBase("10.1.7.0", "255.255.255.0");
    Ipv4InterfaceContainer i3i6 = address.Assign(d3n6);

    address.SetBase("10.1.8.0", "255.255.255.0");
    Ipv4InterfaceContainer csmaIfs = address.Assign(csmaDevices);

    // Application configuration: PacketSink on node 6
    uint16_t port = 5000;
    Address sinkAddress(InetSocketAddress(i5i6.GetAddress(1), port));
    PacketSinkHelper sinkHelper("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApp = sinkHelper.Install(nodes.Get(6));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(25.0));

    // CBR source 1: Node 1 to Node 6, start at 1s (primary path: via 1-4-5-6)
    OnOffHelper onoff1("ns3::UdpSocketFactory", sinkAddress);
    onoff1.SetAttribute("DataRate", StringValue("2Mbps"));
    onoff1.SetAttribute("PacketSize", UintegerValue(512));
    onoff1.SetAttribute("StartTime", TimeValue(Seconds(1.0)));
    onoff1.SetAttribute("StopTime", TimeValue(Seconds(10.0)));
    ApplicationContainer cbrApp1 = onoff1.Install(nodes.Get(1));

    // CBR source 2: Node 1 to Node 6, start at 11s (alternative path: via 1-2-3-6)
    // Use next point-to-point link (n0n1,n2n3,n3n6), must ensure global rerouting
    OnOffHelper onoff2("ns3::UdpSocketFactory", sinkAddress);
    onoff2.SetAttribute("DataRate", StringValue("2Mbps"));
    onoff2.SetAttribute("PacketSize", UintegerValue(512));
    onoff2.SetAttribute("StartTime", TimeValue(Seconds(11.0)));
    onoff2.SetAttribute("StopTime", TimeValue(Seconds(20.0)));
    ApplicationContainer cbrApp2 = onoff2.Install(nodes.Get(1));

    // Tracing sink reception
    sinkApp.Get(0)->TraceConnectWithoutContext("Rx", MakeCallback(&PacketSinkTrace));

    // Tracing queue events on all point-to-point queues
#if NS3_MAJOR_VERSION >= 3 && NS3_MINOR_VERSION >= 38
    for (uint32_t i = 0; i < nodes.GetN(); ++i)
    {
        Ptr<Node> node = nodes.Get(i);
        for (uint32_t j = 0; j < node->GetNDevices(); ++j)
        {
            Ptr<NetDevice> dev = node->GetDevice(j);
            Ptr<PointToPointNetDevice> ptpDev = DynamicCast<PointToPointNetDevice>(dev);
            if (ptpDev)
            {
                Ptr<Queue<Packet>> q = ptpDev->GetQueue();
                if (q)
                {
                    q->TraceConnect("PacketsInQueue", std::to_string(i) + "-" + std::to_string(j),
                                    MakeCallback(&QueueTrace));
                }
            }
        }
    }
#endif

    // Schedule interface down/up and rerouting
    // Bring down interface between node 1 and 4 at 5s, to force rerouting to alt path
    Simulator::Schedule(Seconds(5.0), &InterfaceDown, nodes.Get(1), 2); // p2p index: n1--n4(if 2)
    Simulator::Schedule(Seconds(5.1), &RecomputeRoutes);

    Simulator::Schedule(Seconds(15.0), &InterfaceUp, nodes.Get(1), 2);
    Simulator::Schedule(Seconds(15.1), &RecomputeRoutes);

    // Another event: Bring down CSMA interface on node 2 at 12s (simulate further rerouting)
    Simulator::Schedule(Seconds(12.0), &InterfaceDown, nodes.Get(2), 1); // CSMA interface index on node 2 = 1
    Simulator::Schedule(Seconds(12.1), &RecomputeRoutes);
    Simulator::Schedule(Seconds(18.0), &InterfaceUp, nodes.Get(2), 1);
    Simulator::Schedule(Seconds(18.1), &RecomputeRoutes);

    // Enable static global route recomputation on interface events (since ns-3.38)
    Config::SetDefault("ns3::Ipv4GlobalRouting::RespondToInterfaceEvents", BooleanValue(true));

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    Simulator::Stop(Seconds(25.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}