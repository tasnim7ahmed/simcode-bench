/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/ipv4-routing-table-entry.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/ripv2-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("DvrSimulationExample");

int
main(int argc, char *argv[])
{
    // Enable logging, optional
    // LogComponentEnable("DvrSimulationExample", LOG_LEVEL_INFO);

    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    // Create nodes:
    // R1, R2, R3: routers
    // N1, N2, N3, N4: subnetwork endpoints (could be hosts)
    NodeContainer routers;
    routers.Create(3); // R1:0, R2:1, R3:2

    NodeContainer sub1; sub1.Create(1); // N1
    NodeContainer sub2; sub2.Create(1); // N2
    NodeContainer sub3; sub3.Create(1); // N3
    NodeContainer sub4; sub4.Create(1); // N4

    // Connect subnetworks to routers:
    // sub1---R1---R2---R3---sub4
    //       |     |    |
    //      sub2  sub3  sub4

    // sub1 <--> R1
    NodeContainer n1r1(sub1.Get(0), routers.Get(0));
    // sub2 <--> R1
    NodeContainer n2r1(sub2.Get(0), routers.Get(0));
    // R1 <--> R2
    NodeContainer r1r2(routers.Get(0), routers.Get(1));
    // R2 <--> R3
    NodeContainer r2r3(routers.Get(1), routers.Get(2));
    // sub3 <--> R2
    NodeContainer n3r2(sub3.Get(0), routers.Get(1));
    // sub4 <--> R3
    NodeContainer n4r3(sub4.Get(0), routers.Get(2));

    // Install Internet stack with RIPng v2 on routers
    InternetStackHelper internet;
    RipV2RoutingHelper ripv2;
    Ipv4ListRoutingHelper listRH;
    listRH.Add(ripv2, 0); // RIPV2 as prioritized protocol
    internet.SetRoutingHelper(listRH);

    // Install routing on routers
    internet.Install(routers);

    // Hosts (N1..N4) use default (static) routing
    InternetStackHelper internetNoRouting;
    internetNoRouting.Install(sub1);
    internetNoRouting.Install(sub2);
    internetNoRouting.Install(sub3);
    internetNoRouting.Install(sub4);

    // Point-to-point channel helpers
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // Install NetDevices and assign addresses
    NetDeviceContainer d_n1r1 = p2p.Install(n1r1);
    NetDeviceContainer d_n2r1 = p2p.Install(n2r1);
    NetDeviceContainer d_r1r2 = p2p.Install(r1r2);
    NetDeviceContainer d_r2r3 = p2p.Install(r2r3);
    NetDeviceContainer d_n3r2 = p2p.Install(n3r2);
    NetDeviceContainer d_n4r3 = p2p.Install(n4r3);

    // Assign IP addresses
    Ipv4AddressHelper ipv4;
    // Each link gets a /30 or /29 subnet

    // sub1--R1
    ipv4.SetBase("10.0.1.0", "255.255.255.252");
    Ipv4InterfaceContainer i_n1r1 = ipv4.Assign(d_n1r1);
    // sub2--R1
    ipv4.SetBase("10.0.2.0", "255.255.255.252");
    Ipv4InterfaceContainer i_n2r1 = ipv4.Assign(d_n2r1);
    // R1--R2
    ipv4.SetBase("10.0.3.0", "255.255.255.252");
    Ipv4InterfaceContainer i_r1r2 = ipv4.Assign(d_r1r2);
    // R2--R3
    ipv4.SetBase("10.0.4.0", "255.255.255.252");
    Ipv4InterfaceContainer i_r2r3 = ipv4.Assign(d_r2r3);
    // sub3--R2
    ipv4.SetBase("10.0.5.0", "255.255.255.252");
    Ipv4InterfaceContainer i_n3r2 = ipv4.Assign(d_n3r2);
    // sub4--R3
    ipv4.SetBase("10.0.6.0", "255.255.255.252");
    Ipv4InterfaceContainer i_n4r3 = ipv4.Assign(d_n4r3);

    // Set up the interfaces by bringing them up (optional, for debugging)
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Hosts: set default routes via their router interface
    Ptr<Ipv4StaticRouting> host1StaticRouting = Ipv4RoutingHelper::GetRouting <Ipv4StaticRouting>(sub1.Get(0)->GetObject<Ipv4>()->GetRoutingProtocol());
    host1StaticRouting->SetDefaultRoute(i_n1r1.GetAddress(1), 1);

    Ptr<Ipv4StaticRouting> host2StaticRouting = Ipv4RoutingHelper::GetRouting <Ipv4StaticRouting>(sub2.Get(0)->GetObject<Ipv4>()->GetRoutingProtocol());
    host2StaticRouting->SetDefaultRoute(i_n2r1.GetAddress(1), 1);

    Ptr<Ipv4StaticRouting> host3StaticRouting = Ipv4RoutingHelper::GetRouting <Ipv4StaticRouting>(sub3.Get(0)->GetObject<Ipv4>()->GetRoutingProtocol());
    host3StaticRouting->SetDefaultRoute(i_n3r2.GetAddress(1), 1);

    Ptr<Ipv4StaticRouting> host4StaticRouting = Ipv4RoutingHelper::GetRouting <Ipv4StaticRouting>(sub4.Get(0)->GetObject<Ipv4>()->GetRoutingProtocol());
    host4StaticRouting->SetDefaultRoute(i_n4r3.GetAddress(1), 1);

    // Enable pcap tracing for router and host interfaces
    p2p.EnablePcapAll("dvr-example");

    // Install applications to test routing -- ping from N1 to N4, N2 to N3, etc.
    uint16_t port = 9;

    // N1 -> N4
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApps = echoServer.Install(sub4.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(20.0));
    
    UdpEchoClientHelper echoClient(i_n4r3.GetAddress(0), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(2.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(64));

    ApplicationContainer clientApps = echoClient.Install(sub1.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(20.0));

    // N2 -> N3
    UdpEchoServerHelper echoServer2(port + 1);
    serverApps = echoServer2.Install(sub3.Get(0));
    serverApps.Start(Seconds(1.5));
    serverApps.Stop(Seconds(20.0));

    UdpEchoClientHelper echoClient2(i_n3r2.GetAddress(0), port + 1);
    echoClient2.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient2.SetAttribute("Interval", TimeValue(Seconds(2.5)));
    echoClient2.SetAttribute("PacketSize", UintegerValue(64));
    clientApps = echoClient2.Install(sub2.Get(0));
    clientApps.Start(Seconds(3.0));
    clientApps.Stop(Seconds(20.0));

    // Schedule the routing table prints for each router
    for (uint32_t i = 0; i < routers.GetN(); ++i)
    {
        Ptr<Node> router = routers.Get(i);
        Simulator::Schedule(Seconds(5.0 + i), &Ipv4GlobalRoutingHelper::PrintRoutingTableAllAt, Seconds(10.0 + i), router->GetObject<Node>());
    }

    Simulator::Stop(Seconds(22.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}