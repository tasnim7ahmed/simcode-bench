#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/ipv6-routing-helper.h"
#include "ns3/ipv6-list-routing-helper.h"
#include "ns3/ipv6-address-helper.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("LooseSourceIpV6RoutingExample");

int main(int argc, char *argv[])
{
    // Enable logging (if desired)
    //LogComponentEnable("LooseSourceIpV6RoutingExample", LOG_LEVEL_INFO);
    // Command line arguments (none here)
    CommandLine cmd;
    cmd.Parse(argc, argv);

    // Create nodes: Host0, Host1, Router0, Router1, Router2, Router3
    NodeContainer hosts;
    hosts.Create(2); // Host0, Host1

    NodeContainer routers;
    routers.Create(4); // Router0, Router1, Router2, Router3

    // Naming nodes for clarity
    Ptr<Node> host0 = hosts.Get(0);
    Ptr<Node> host1 = hosts.Get(1);
    Ptr<Node> router0 = routers.Get(0);
    Ptr<Node> router1 = routers.Get(1);
    Ptr<Node> router2 = routers.Get(2);
    Ptr<Node> router3 = routers.Get(3);

    // Links:
    // Host0 <-> Router0 <-> Router1 <-> Router2 <-> Router3 <-> Host1
    NodeContainer h0r0; h0r0.Add(host0); h0r0.Add(router0);
    NodeContainer r0r1; r0r1.Add(router0); r0r1.Add(router1);
    NodeContainer r1r2; r1r2.Add(router1); r1r2.Add(router2);
    NodeContainer r2r3; r2r3.Add(router2); r2r3.Add(router3);
    NodeContainer r3h1; r3h1.Add(router3); r3h1.Add(host1);

    // Install point-to-point channels
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer d_h0r0 = p2p.Install(h0r0);
    NetDeviceContainer d_r0r1 = p2p.Install(r0r1);
    NetDeviceContainer d_r1r2 = p2p.Install(r1r2);
    NetDeviceContainer d_r2r3 = p2p.Install(r2r3);
    NetDeviceContainer d_r3h1 = p2p.Install(r3h1);

    // Install stack
    InternetStackHelper internetv6;
    Ipv6ListRoutingHelper listRouting;
    Ipv6StaticRoutingHelper staticRouting;
    listRouting.Add(staticRouting, 0);
    internetv6.SetRoutingHelper(listRouting);
    internetv6.Install(host0);
    internetv6.Install(host1);
    internetv6.Install(routers);

    // Assign IPv6 addresses
    Ipv6AddressHelper ipv6;
    ipv6.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer if_h0r0 = ipv6.Assign(d_h0r0);

    ipv6.SetBase(Ipv6Address("2001:2::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer if_r0r1 = ipv6.Assign(d_r0r1);

    ipv6.SetBase(Ipv6Address("2001:3::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer if_r1r2 = ipv6.Assign(d_r1r2);

    ipv6.SetBase(Ipv6Address("2001:4::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer if_r2r3 = ipv6.Assign(d_r2r3);

    ipv6.SetBase(Ipv6Address("2001:5::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer if_r3h1 = ipv6.Assign(d_r3h1);

    // Enable forwarding on router interfaces
    for (uint32_t i = 0; i < routers.GetN(); ++i)
    {
        Ptr<Ipv6> ipv6 = routers.Get(i)->GetObject<Ipv6>();
        for (uint32_t j = 0; j < ipv6->GetNInterfaces(); ++j)
        {
            ipv6->SetForwarding(j, true);
            ipv6->SetDefaultRoute(j);
        }
    }

    // Assign global addresses and set default routes on hosts
    Ptr<Ipv6StaticRouting> h0StaticRouting = Ipv6RoutingHelper::GetRouting <Ipv6StaticRouting>(host0->GetObject<Ipv6>()->GetRoutingProtocol());
    Ptr<Ipv6StaticRouting> h1StaticRouting = Ipv6RoutingHelper::GetRouting <Ipv6StaticRouting>(host1->GetObject<Ipv6>()->GetRoutingProtocol());

    // Set default routes to respective router
    h0StaticRouting->SetDefaultRoute(if_h0r0.GetAddress(1, 1), 1);
    h1StaticRouting->SetDefaultRoute(if_r3h1.GetAddress(0, 1), 1);

    // For loose source routing using IPv6 Routing Header (Type 0 Routing Header is deprecated/RFC5095)
    // But in ns-3, flexibility is limited; we simulate behavior with static routes.
    // We'll add static routes at each router for this flow

    // Host0's source address & Host1's destination address
    Ipv6Address h0Addr = if_h0r0.GetAddress(0, 1);
    Ipv6Address h1Addr = if_r3h1.GetAddress(1, 1);

    // Static routes on routers
    Ptr<Ipv6StaticRouting> r0Static = Ipv6RoutingHelper::GetRouting<Ipv6StaticRouting>(router0->GetObject<Ipv6>()->GetRoutingProtocol());
    Ptr<Ipv6StaticRouting> r1Static = Ipv6RoutingHelper::GetRouting<Ipv6StaticRouting>(router1->GetObject<Ipv6>()->GetRoutingProtocol());
    Ptr<Ipv6StaticRouting> r2Static = Ipv6RoutingHelper::GetRouting<Ipv6StaticRouting>(router2->GetObject<Ipv6>()->GetRoutingProtocol());
    Ptr<Ipv6StaticRouting> r3Static = Ipv6RoutingHelper::GetRouting<Ipv6StaticRouting>(router3->GetObject<Ipv6>()->GetRoutingProtocol());

    // Simulate loose source routing: Host0 -> Router0 -> Router1 -> Router2 -> Router3 -> Host1

    // Router0: Forward to Router1 via interface 2 (r0r1)
    r0Static->AddHostRouteTo(h1Addr, if_r0r1.GetAddress(1,1), 2);

    // Router1: Forward to Router2 via interface 2 (r1r2)
    r1Static->AddHostRouteTo(h1Addr, if_r1r2.GetAddress(1,1), 2);

    // Router2: Forward to Router3 via interface 2 (r2r3)
    r2Static->AddHostRouteTo(h1Addr, if_r2r3.GetAddress(1,1), 2);

    // Router3: Forward to Host1 via interface 2 (r3h1)
    r3Static->AddHostRouteTo(h1Addr, if_r3h1.GetAddress(1,1), 2);

    // Application: ICMPv6 echo request from Host0 to Host1
    uint16_t echoPort = 9999;

    // Install ICMPv6 Echo Server on Host1
    Icmpv6EchoServerHelper echoServerHelper(echoPort);
    ApplicationContainer serverApps = echoServerHelper.Install(host1);
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // Install ICMPv6 Echo Client on Host0
    Icmpv6EchoClientHelper echoClientHelper(h1Addr, echoPort);
    echoClientHelper.SetAttribute("MaxPackets", UintegerValue(5));
    echoClientHelper.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClientHelper.SetAttribute("PacketSize", UintegerValue(64));
    ApplicationContainer clientApps = echoClientHelper.Install(host0);
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Enable packet tracing
    AsciiTraceHelper ascii;
    p2p.EnableAsciiAll(ascii.CreateFileStream("loose-routing-ipv6.tr"));

    Simulator::Stop(Seconds(11.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}