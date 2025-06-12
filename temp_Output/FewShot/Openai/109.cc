#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable packet capture and set up logging
    LogComponentEnable("Ping6Application", LOG_LEVEL_INFO);

    // Create nodes: Host0, Host1, R0, R1, R2, R3
    NodeContainer hosts;
    hosts.Create(2); // Hosts 0 and 1
    NodeContainer routers;
    routers.Create(4); // Routers 0-3

    Ptr<Node> host0 = hosts.Get(0);
    Ptr<Node> host1 = hosts.Get(1);
    Ptr<Node> r0 = routers.Get(0);
    Ptr<Node> r1 = routers.Get(1);
    Ptr<Node> r2 = routers.Get(2);
    Ptr<Node> r3 = routers.Get(3);

    // Topology:
    // Host0 <-> R0 <-> R1 <-> R2 <-> R3 <-> Host1

    // Link containers
    NodeContainer h0r0(host0, r0);
    NodeContainer r0r1(r0, r1);
    NodeContainer r1r2(r1, r2);
    NodeContainer r2r3(r2, r3);
    NodeContainer r3h1(r3, host1);

    // Point-to-point links
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer d_h0r0 = p2p.Install(h0r0);
    NetDeviceContainer d_r0r1 = p2p.Install(r0r1);
    NetDeviceContainer d_r1r2 = p2p.Install(r1r2);
    NetDeviceContainer d_r2r3 = p2p.Install(r2r3);
    NetDeviceContainer d_r3h1 = p2p.Install(r3h1);

    // Install the Internet stack with IPv6 support
    InternetStackHelper stack;
    stack.Install(host0);
    stack.Install(host1);
    stack.Install(routers);

    // Assign IPv6 addresses
    Ipv6AddressHelper ipv6;

    ipv6.SetBase("2001:1::", 64);
    Ipv6InterfaceContainer i_h0r0 = ipv6.Assign(d_h0r0);

    ipv6.SetBase("2001:2::", 64);
    Ipv6InterfaceContainer i_r0r1 = ipv6.Assign(d_r0r1);

    ipv6.SetBase("2001:3::", 64);
    Ipv6InterfaceContainer i_r1r2 = ipv6.Assign(d_r1r2);

    ipv6.SetBase("2001:4::", 64);
    Ipv6InterfaceContainer i_r2r3 = ipv6.Assign(d_r2r3);

    ipv6.SetBase("2001:5::", 64);
    Ipv6InterfaceContainer i_r3h1 = ipv6.Assign(d_r3h1);

    // Enable forwarding on routers
    for (uint32_t i = 0; i < routers.GetN(); ++i) {
        Ptr<Ipv6> ipv6 = routers.Get(i)->GetObject<Ipv6>();
        ipv6->SetAttribute("IpForward", BooleanValue(true));
    }

    // Set up static routing with loose source routing
    Ipv6StaticRoutingHelper ipv6RoutingHelper;

    Ptr<Ipv6StaticRouting> host0Routing = ipv6RoutingHelper.GetStaticRouting(host0->GetObject<Ipv6>());
    Ptr<Ipv6StaticRouting> host1Routing = ipv6RoutingHelper.GetStaticRouting(host1->GetObject<Ipv6>());

    // Get Host1's IPv6 address (final destination)
    Ipv6Address host1Addr = i_r3h1.GetAddress(1,1); // Host1 side, global address

    // Routers' interface addresses for loose source routing
    Ipv6Address r0Addr = i_r0r1.GetAddress(0,1); // R0, interface towards R1
    Ipv6Address r2Addr = i_r1r2.GetAddress(1,1); // R2, interface towards R3

    // Host0 default route set to R0 (next hop)
    host0Routing->SetDefaultRoute(i_h0r0.GetAddress(1,1), 1);

    // Host1 default route set to R3 (next hop)
    host1Routing->SetDefaultRoute(i_r3h1.GetAddress(0,1), 1);

    // Set up routing between routers (using static routing for demonstration - production would use dynamic)
    Ptr<Ipv6StaticRouting> r0Routing = ipv6RoutingHelper.GetStaticRouting(r0->GetObject<Ipv6>());
    Ptr<Ipv6StaticRouting> r1Routing = ipv6RoutingHelper.GetStaticRouting(r1->GetObject<Ipv6>());
    Ptr<Ipv6StaticRouting> r2Routing = ipv6RoutingHelper.GetStaticRouting(r2->GetObject<Ipv6>());
    Ptr<Ipv6StaticRouting> r3Routing = ipv6RoutingHelper.GetStaticRouting(r3->GetObject<Ipv6>());

    // Set up routes between routers
    // R0: aware of Host1 via R1
    r0Routing->AddNetworkRouteTo(Ipv6Address("2001:5::"), Ipv6Prefix(64), i_r0r1.GetAddress(1,1), 2);

    // R1: aware of Host1 via R2
    r1Routing->AddNetworkRouteTo(Ipv6Address("2001:5::"), Ipv6Prefix(64), i_r1r2.GetAddress(1,1), 2);

    // R2: aware of Host1 via R3
    r2Routing->AddNetworkRouteTo(Ipv6Address("2001:5::"), Ipv6Prefix(64), i_r2r3.GetAddress(1,1), 2);

    // R3: aware of Host1 directly
    r3Routing->AddNetworkRouteTo(Ipv6Address("2001:5::"), Ipv6Prefix(64), i_r3h1.GetAddress(1,1), 2);

    // Set up loose source routing options for Ping6
    Ping6Helper ping6(host1Addr);
    ping6.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    ping6.SetAttribute("PacketSize", UintegerValue(56));
    ping6.SetAttribute("MaxPackets", UintegerValue(5));
    ping6.SetAttribute("Verbose", BooleanValue(true));

    // Set up the loose source route: [R0, R2, Host1]
    std::vector<Ipv6Address> route;
    route.push_back(r0Addr);
    route.push_back(r2Addr);
    route.push_back(host1Addr);

    ping6.SetAttribute("SourceRoute", Ipv6AddressVectorValue(route));

    ApplicationContainer pingApps = ping6.Install(host0);
    pingApps.Start(Seconds(2.0));
    pingApps.Stop(Seconds(10.0));

    // Enable packet capture/tracing
    AsciiTraceHelper ascii;
    p2p.EnableAsciiAll(ascii.CreateFileStream("loose-routing-ipv6.tr"));

    Simulator::Stop(Seconds(11.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}