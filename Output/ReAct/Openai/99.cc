#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

int
main(int argc, char *argv[])
{
    CommandLine cmd;
    cmd.Parse(argc, argv);

    // Create nodes: A, B, C
    NodeContainer nodes;
    nodes.Create(3);
    Ptr<Node> nodeA = nodes.Get(0);
    Ptr<Node> nodeB = nodes.Get(1);
    Ptr<Node> nodeC = nodes.Get(2);

    // Create point-to-point links: A-B and B-C
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // Link A-B
    NodeContainer ab(nodeA, nodeB);
    NetDeviceContainer dev_ab = p2p.Install(ab);

    // Link B-C
    NodeContainer bc(nodeB, nodeC);
    NetDeviceContainer dev_bc = p2p.Install(bc);

    // Install internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.0.1.0", "255.255.255.0");
    Ipv4InterfaceContainer iface_ab = ipv4.Assign(dev_ab);

    ipv4.SetBase("10.0.2.0", "255.255.255.0");
    Ipv4InterfaceContainer iface_bc = ipv4.Assign(dev_bc);

    // Assign unique /32 addresses to A and C
    Ptr<Ipv4> ipv4A = nodeA->GetObject<Ipv4>();
    Ptr<Ipv4> ipv4B = nodeB->GetObject<Ipv4>();
    Ptr<Ipv4> ipv4C = nodeC->GetObject<Ipv4>();

    // Overwrite assigned addresses to guarantee /32 (host route) and correctness
    ipv4A->SetMetric(1, 1);
    ipv4B->SetMetric(1, 1);
    ipv4C->SetMetric(1, 1);

    Ipv4InterfaceAddress ifAddr_A = ipv4A->GetAddress(1,0);
    Ipv4InterfaceAddress ifAddr_B_ab = ipv4B->GetAddress(1,0);
    Ipv4InterfaceAddress ifAddr_B_bc = ipv4B->GetAddress(2,0);
    Ipv4InterfaceAddress ifAddr_C = ipv4C->GetAddress(1,0);

    // Remove old addresses and assign /32 addresses on A and C
    ipv4A->RemoveAddress(1,0);
    ipv4A->AddAddress(1, Ipv4InterfaceAddress("10.0.1.10", "255.255.255.255"));
    ipv4A->SetUp(1);

    ipv4C->RemoveAddress(1,0);
    ipv4C->AddAddress(1, Ipv4InterfaceAddress("10.0.2.20", "255.255.255.255"));
    ipv4C->SetUp(1);

    // Reassign for B (leave interfaces using /24 as transit)
    // Already set by Assign()

    // Routing configuration

    // Global routing on all nodes
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Now inject static/host routes at node B
    Ipv4StaticRoutingHelper staticRoutingHelper;
    Ptr<Ipv4StaticRouting> staticRoutingB = staticRoutingHelper.GetStaticRouting(nodeB->GetObject<Ipv4>());

    // Route from B to A's /32
    staticRoutingB->AddHostRouteTo(
        Ipv4Address("10.0.1.10"),
        Ipv4Address("10.0.1.10"),
        1
    );
    // Route from B to C's /32
    staticRoutingB->AddHostRouteTo(
        Ipv4Address("10.0.2.20"),
        Ipv4Address("10.0.2.20"),
        2
    );

    // Configure OnOff UDP application on A to C
    uint16_t port = 5000;
    OnOffHelper onoff("ns3::UdpSocketFactory",
                      Address(InetSocketAddress(Ipv4Address("10.0.2.20"), port)));
    onoff.SetConstantRate(DataRate("1Mbps"));
    onoff.SetAttribute("StartTime", TimeValue(Seconds(1.0)));
    onoff.SetAttribute("StopTime", TimeValue(Seconds(9.0)));

    ApplicationContainer app = onoff.Install(nodeA);

    // UDP packet sink on C
    PacketSinkHelper sink("ns3::UdpSocketFactory",
                          Address(InetSocketAddress(Ipv4Address::GetAny(), port)));
    sink.Install(nodeC);

    // Enable PCAP tracing
    p2p.EnablePcapAll("three-router");

    // Packet tracing
    AsciiTraceHelper ascii;
    p2p.EnableAsciiAll(ascii.CreateFileStream("three-router.tr"));

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}