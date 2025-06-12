#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("MultiInterfaceHostStaticRoutingExample");

int main(int argc, char *argv[])
{
    CommandLine cmd;
    cmd.Parse(argc, argv);

    // Enable logging for tracing
    LogComponentEnable("MultiInterfaceHostStaticRoutingExample", LOG_LEVEL_INFO);

    // Create nodes
    // [Src]--p2p--[Rtr1]          [Rtr2]--p2p--[DstRtr]--p2p--[Dst]
    //     \        /                 /
    //      --p2p--/
    NodeContainer sourceNode; // host with two interfaces
    sourceNode.Create(1);

    NodeContainer rtr1, rtr2, dstRtr, dstNode;
    rtr1.Create(1);
    rtr2.Create(1);
    dstRtr.Create(1);
    dstNode.Create(1);

    // Links:
    // Source <--> Rtr1 (Subnet 1)
    // Source <--> Rtr2 (Subnet 2)
    // Rtr1 <--> DstRtr (Subnet 3)
    // Rtr2 <--> DstRtr (Subnet 4)
    // DstRtr <--> Dst (Subnet 5)
    NodeContainer src_rtr1 = NodeContainer(sourceNode.Get(0), rtr1.Get(0));
    NodeContainer src_rtr2 = NodeContainer(sourceNode.Get(0), rtr2.Get(0));
    NodeContainer rtr1_dstRtr = NodeContainer(rtr1.Get(0), dstRtr.Get(0));
    NodeContainer rtr2_dstRtr = NodeContainer(rtr2.Get(0), dstRtr.Get(0));
    NodeContainer dstRtr_dst = NodeContainer(dstRtr.Get(0), dstNode.Get(0));

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(sourceNode);
    stack.Install(rtr1);
    stack.Install(rtr2);
    stack.Install(dstRtr);
    stack.Install(dstNode);

    // Create point-to-point channels
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer d_src_rtr1 = p2p.Install(src_rtr1);
    NetDeviceContainer d_src_rtr2 = p2p.Install(src_rtr2);
    NetDeviceContainer d_rtr1_dstRtr = p2p.Install(rtr1_dstRtr);
    NetDeviceContainer d_rtr2_dstRtr = p2p.Install(rtr2_dstRtr);
    NetDeviceContainer d_dstRtr_dst = p2p.Install(dstRtr_dst);

    // Assign IP addresses
    // Assign subnets: 10.0.1.0, 10.0.2.0, 10.0.3.0, 10.0.4.0, 10.0.5.0/24
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.0.1.0", "255.255.255.0");
    Ipv4InterfaceContainer i_src_rtr1 = ipv4.Assign(d_src_rtr1);

    ipv4.SetBase("10.0.2.0", "255.255.255.0");
    Ipv4InterfaceContainer i_src_rtr2 = ipv4.Assign(d_src_rtr2);

    ipv4.SetBase("10.0.3.0", "255.255.255.0");
    Ipv4InterfaceContainer i_rtr1_dstRtr = ipv4.Assign(d_rtr1_dstRtr);

    ipv4.SetBase("10.0.4.0", "255.255.255.0");
    Ipv4InterfaceContainer i_rtr2_dstRtr = ipv4.Assign(d_rtr2_dstRtr);

    ipv4.SetBase("10.0.5.0", "255.255.255.0");
    Ipv4InterfaceContainer i_dstRtr_dst = ipv4.Assign(d_dstRtr_dst);

    // Get Ipv4 objects
    Ptr<Ipv4> srcIpv4 = sourceNode.Get(0)->GetObject<Ipv4>();
    Ptr<Ipv4> rtr1Ipv4 = rtr1.Get(0)->GetObject<Ipv4>();
    Ptr<Ipv4> rtr2Ipv4 = rtr2.Get(0)->GetObject<Ipv4>();
    Ptr<Ipv4> dstRtrIpv4 = dstRtr.Get(0)->GetObject<Ipv4>();
    Ptr<Ipv4> dstIpv4 = dstNode.Get(0)->GetObject<Ipv4>();

    // Enable static routing
    Ipv4StaticRoutingHelper staticRoutingHelper;

    // On sourceNode, add static routes to destination via Rtr1 and Rtr2
    // Destination is 10.0.5.2 (Dst node)

    // Set routes on source node for each interface
    // interface 1: first net device (to Rtr1, subnet 10.0.1.0)
    // interface 2: second net device (to Rtr2, subnet 10.0.2.0)
    Ptr<Ipv4StaticRouting> sourceStaticRouting = staticRoutingHelper.GetStaticRouting(srcIpv4);
    // Via Rtr1 (to DstNet via Rtr1)
    sourceStaticRouting->AddNetworkRouteTo(Ipv4Address("10.0.5.0"), Ipv4Mask("255.255.255.0"), 
                                           i_src_rtr1.GetAddress(1), 1);

    // Via Rtr2 (to DstNet via Rtr2)
    sourceStaticRouting->AddNetworkRouteTo(Ipv4Address("10.0.5.0"), Ipv4Mask("255.255.255.0"),
                                           i_src_rtr2.GetAddress(1), 2);

    // Routers need static routes to destination as well

    // Rtr1: route to DstNet via DstRtr through interface to DstRtr
    Ptr<Ipv4StaticRouting> rtr1StaticRouting = staticRoutingHelper.GetStaticRouting(rtr1Ipv4);
    rtr1StaticRouting->AddNetworkRouteTo(Ipv4Address("10.0.5.0"), Ipv4Mask("255.255.255.0"),
                                         i_rtr1_dstRtr.GetAddress(1), 2); // interface towards DstRtr

    // Rtr2: route to DstNet via DstRtr
    Ptr<Ipv4StaticRouting> rtr2StaticRouting = staticRoutingHelper.GetStaticRouting(rtr2Ipv4);
    rtr2StaticRouting->AddNetworkRouteTo(Ipv4Address("10.0.5.0"), Ipv4Mask("255.255.255.0"),
                                         i_rtr2_dstRtr.GetAddress(1), 2); // interface towards DstRtr

    // DstRtr: route to DstNet is direct (directly connected)
    // DstRtr: Add reverse routes to SrcNet via Rtr1 and via Rtr2 for responses
    Ptr<Ipv4StaticRouting> dstRtrStaticRouting = staticRoutingHelper.GetStaticRouting(dstRtrIpv4);
    // For returning to 10.0.1.0 (Src <-> Rtr1)
    dstRtrStaticRouting->AddNetworkRouteTo(Ipv4Address("10.0.1.0"), Ipv4Mask("255.255.255.0"),
                                           i_rtr1_dstRtr.GetAddress(0), 1);
    // For returning to 10.0.2.0 (Src <-> Rtr2)
    dstRtrStaticRouting->AddNetworkRouteTo(Ipv4Address("10.0.2.0"), Ipv4Mask("255.255.255.0"),
                                           i_rtr2_dstRtr.GetAddress(0), 2);

    // Rtr1: return route to SrcNet via source
    rtr1StaticRouting->AddNetworkRouteTo(Ipv4Address("10.0.1.0"), Ipv4Mask("255.255.255.0"),
                                         i_src_rtr1.GetAddress(0), 1);

    rtr2StaticRouting->AddNetworkRouteTo(Ipv4Address("10.0.2.0"), Ipv4Mask("255.255.255.0"),
                                         i_src_rtr2.GetAddress(0), 1);

    // Dst node: default route via gateway
    Ptr<Ipv4StaticRouting> dstStaticRouting = staticRoutingHelper.GetStaticRouting(dstIpv4);
    dstStaticRouting->SetDefaultRoute(i_dstRtr_dst.GetAddress(0), 1);

    // Rtr1: default route to DstRtr via DstRtr link
    rtr1StaticRouting->SetDefaultRoute(i_rtr1_dstRtr.GetAddress(1), 2);

    // Rtr2: default route to DstRtr via DstRtr link
    rtr2StaticRouting->SetDefaultRoute(i_rtr2_dstRtr.GetAddress(1), 2);

    // DstRtr: default route to DstNet via link to Dst
    dstRtrStaticRouting->SetDefaultRoute(i_dstRtr_dst.GetAddress(1), 3);

    // Source node: (not strictly necessary as static routing added)

    // Set up TCP servers and clients
    uint16_t port = 50000;
    Address sinkAddress(InetSocketAddress(i_dstRtr_dst.GetAddress(1), port));

    // Application: TCP Sink on Dst node
    PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApps = sinkHelper.Install(dstNode.Get(0));
    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(Seconds(10.0));

    // 1st TCP flow from Src to Dst via Rtr1
    OnOffHelper clientHelper1("ns3::TcpSocketFactory", sinkAddress);
    clientHelper1.SetAttribute("DataRate", StringValue("1Mbps"));
    clientHelper1.SetAttribute("PacketSize", UintegerValue(1024));
    clientHelper1.SetAttribute("StartTime", TimeValue(Seconds(1.0)));
    clientHelper1.SetAttribute("StopTime", TimeValue(Seconds(3.0)));

    // To force the source to use outgoing interface 1 (going via Rtr1), we must bind the source address appropriately
    clientHelper1.SetAttribute("Local", AddressValue(InetSocketAddress(i_src_rtr1.GetAddress(0), 0)));
    ApplicationContainer clientApps1 = clientHelper1.Install(sourceNode.Get(0));

    // 2nd TCP flow from Src to Dst via Rtr2
    OnOffHelper clientHelper2("ns3::TcpSocketFactory", sinkAddress);
    clientHelper2.SetAttribute("DataRate", StringValue("1Mbps"));
    clientHelper2.SetAttribute("PacketSize", UintegerValue(1024));
    clientHelper2.SetAttribute("StartTime", TimeValue(Seconds(4.0)));
    clientHelper2.SetAttribute("StopTime", TimeValue(Seconds(6.0)));

    // Bind source address to the interface facing Rtr2
    clientHelper2.SetAttribute("Local", AddressValue(InetSocketAddress(i_src_rtr2.GetAddress(0), 0)));
    ApplicationContainer clientApps2 = clientHelper2.Install(sourceNode.Get(0));

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}