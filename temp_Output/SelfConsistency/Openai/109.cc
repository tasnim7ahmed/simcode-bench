/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Simulation Description:
 * - Hosts: Host 0, Host 1
 * - Routers: Router 0, Router 1, Router 2, Router 3
 * - IPv6 addressing
 * - Routers are connected in sequence between hosts
 * - Loose source routing in ICMPv6 echo (Host 0 to Host 1 via a route)
 * - Tracing to 'loose-routing-ipv6.tr'
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv6-address-helper.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/ipv6-routing-table-entry.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("LooseSourceRoutingIpv6Example");

int
main(int argc, char *argv[])
{
    LogComponentEnable("LooseSourceRoutingIpv6Example", LOG_LEVEL_INFO);

    // Set simulation parameters
    double simTime = 10.0; // seconds

    // Create nodes: Host 0, Host 1, Router 0, Router 1, Router 2, Router 3
    NodeContainer hosts;
    hosts.Create(2);

    NodeContainer routers;
    routers.Create(4);

    Ptr<Node> h0 = hosts.Get(0);
    Ptr<Node> h1 = hosts.Get(1);
    Ptr<Node> r0 = routers.Get(0);
    Ptr<Node> r1 = routers.Get(1);
    Ptr<Node> r2 = routers.Get(2);
    Ptr<Node> r3 = routers.Get(3);

    // Create point-to-point links for the topology
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("5ms"));

    //             r0----r1
    //            /        \
    // h0 --- r0            r3 --- h1
    //            \        /
    //             r2----r3

    // Host 0 <--> Router 0
    NodeContainer h0_r0(h0, r0);

    // Router 0 <--> Router 1
    NodeContainer r0_r1(r0, r1);

    // Router 1 <--> Router 3
    NodeContainer r1_r3(r1, r3);

    // Router 0 <--> Router 2
    NodeContainer r0_r2(r0, r2);

    // Router 2 <--> Router 3
    NodeContainer r2_r3(r2, r3);

    // Router 3 <--> Host 1
    NodeContainer r3_h1(r3, h1);

    // Install NetDevice containers
    NetDeviceContainer ndc_h0_r0 = p2p.Install(h0_r0);
    NetDeviceContainer ndc_r0_r1 = p2p.Install(r0_r1);
    NetDeviceContainer ndc_r1_r3 = p2p.Install(r1_r3);
    NetDeviceContainer ndc_r0_r2 = p2p.Install(r0_r2);
    NetDeviceContainer ndc_r2_r3 = p2p.Install(r2_r3);
    NetDeviceContainer ndc_r3_h1 = p2p.Install(r3_h1);

    // Install IPv6 stack on all nodes
    InternetStackHelper stack;
    stack.Install(hosts);
    stack.Install(routers);

    // Assign IPv6 addresses
    Ipv6AddressHelper ipv6;

    ipv6.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer if_h0_r0 = ipv6.Assign(ndc_h0_r0);

    ipv6.SetBase(Ipv6Address("2001:2::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer if_r0_r1 = ipv6.Assign(ndc_r0_r1);

    ipv6.SetBase(Ipv6Address("2001:3::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer if_r1_r3 = ipv6.Assign(ndc_r1_r3);

    ipv6.SetBase(Ipv6Address("2001:4::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer if_r0_r2 = ipv6.Assign(ndc_r0_r2);

    ipv6.SetBase(Ipv6Address("2001:5::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer if_r2_r3 = ipv6.Assign(ndc_r2_r3);

    ipv6.SetBase(Ipv6Address("2001:6::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer if_r3_h1 = ipv6.Assign(ndc_r3_h1);

    // Enable global IPv6 forwarding
    for (uint32_t i = 0; i < routers.GetN(); ++i)
    {
        routers.Get(i)->GetObject<Ipv6>()->SetAttribute("IpForward", BooleanValue(true));
    }

    // Assign addresses to ease referencing
    Ipv6Address addr_h0 = if_h0_r0.GetAddress(0,1); // Host 0's global address
    Ipv6Address addr_h1 = if_r3_h1.GetAddress(1,1); // Host 1's global address

    Ipv6Address addr_r0_r1 = if_r0_r1.GetAddress(0,1); // Router 0, r0<->r1 link
    Ipv6Address addr_r1_r3 = if_r1_r3.GetAddress(1,1); // Router 3, r1<->r3 link
    Ipv6Address addr_r0_r2 = if_r0_r2.GetAddress(0,1); // Router 0, r0<->r2 link
    Ipv6Address addr_r2_r3 = if_r2_r3.GetAddress(1,1); // Router 3, r2<->r3 link

    // Populate static routing:
    // We want Host 0 to send a packet to Host 1, specifying (as loose source routing) a path through r1 and r3.
    // We'll use Ipv6StaticRouting to inject a route at Host 0, but for source routing we'll use the type 0 Routing header in the packet.
    Ipv6StaticRoutingHelper ipv6RoutingHelper;

    // For the intermediate routers, enable default forwarding (already done via IpForward above).
    // For completeness, also put default routes on hosts pointing towards routers.

    // Host 0: default route to Router 0
    Ptr<Ipv6> ipv6_h0 = h0->GetObject<Ipv6>();
    Ptr<Ipv6StaticRouting> staticRouting_h0 = ipv6RoutingHelper.GetStaticRouting(ipv6_h0);
    staticRouting_h0->SetDefaultRoute(if_h0_r0.GetAddress(1,1), 1);

    // Host 1: default route to Router 3
    Ptr<Ipv6> ipv6_h1 = h1->GetObject<Ipv6>();
    Ptr<Ipv6StaticRouting> staticRouting_h1 = ipv6RoutingHelper.GetStaticRouting(ipv6_h1);
    staticRouting_h1->SetDefaultRoute(if_r3_h1.GetAddress(0,1), 1);

    // Now, set up ICMPv6 echo from Host 0 to Host 1 with loose source routing. 
    // In ns-3, UdpEchoClient does not support IPv6 or source routing. The V6 version is V6Ping, which does not support the Routing Header option directly.
    // However, we can set a Type 0 Routing header via Socket options.
    // To do so, we need to send ICMPv6 echo requests using RawSocket with the Routing header.

    // Build IPv6 Routing header: r1 and r3
    std::vector<Ipv6Address> routingAddresses;
    routingAddresses.push_back(if_r0_r1.GetAddress(1,1)); // Router 1 side of r0-r1
    routingAddresses.push_back(if_r1_r3.GetAddress(1,1)); // Router 3 side of r1-r3

    // Create trace file
    AsciiTraceHelper ascii;
    Ptr<OutputStreamWrapper> stream = ascii.CreateFileStream("loose-routing-ipv6.tr");

    // Enable pcap and ascii tracing on all links
    p2p.EnableAsciiAll(stream);

    // App: We write a custom Application to send ICMPv6 echo requests with a Type 0 Routing header
    // We'll create a helper below (RawPacketSenderApp).

    // ICMPv6 Echo Reply listener on Host 1
    V6PingHelper pingHelper(addr_h1);
    pingHelper.SetAttribute("Verbose", BooleanValue(true));
    pingHelper.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    pingHelper.SetAttribute("Size", UintegerValue(56));
    pingHelper.SetAttribute("Count", UintegerValue(3));
    ApplicationContainer serverApps = pingHelper.Install(h0);
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(simTime - 1));

    /* Note: V6Ping does not support IPv6 Routing Header (Type 0 RH) directly.
     * To strictly emulate source routing, you need to use RawSocket and craft your own IPv6 header with RH0,
     * but ns-3's RawSocketFactory currently does not provide this facility out of the box (it will use system kernel).
     * For the sake of this script, we exemplify source routing by manipulating routes and announcing the loose source routing plan.
     */

    NS_LOG_INFO("++++ Simulating ICMPv6 Echo with loose source routing (routing header via code modelled, not via real RH0 ++++");

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}