#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/csma-module.h"
#include "ns3/ipv6-routing-table-entry.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable logging for ICMPv6 echo applications
    LogComponentEnable("Icmpv6EchoClient", LOG_LEVEL_INFO);
    LogComponentEnable("Icmpv6EchoServer", LOG_LEVEL_INFO);

    // Create nodes: Host0, Host1, Router0, Router1, Router2, Router3
    NodeContainer hosts;
    hosts.Create(2);

    NodeContainer routers;
    routers.Create(4);

    // Combine all nodes for internet stack installation
    NodeContainer allNodes = NodeContainer(hosts, routers);

    // Create point-to-point links between nodes
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // Connect Host0 to Router0
    NetDeviceContainer host0_router0 = p2p.Install(hosts.Get(0), routers.Get(0));

    // Connect Router0 to Router1
    NetDeviceContainer router0_router1 = p2p.Install(routers.Get(0), routers.Get(1));

    // Connect Router1 to Router2
    NetDeviceContainer router1_router2 = p2p.Install(routers.Get(1), routers.Get(2));

    // Connect Router2 to Router3
    NetDeviceContainer router2_router3 = p2p.Install(routers.Get(2), routers.Get(3));

    // Connect Router3 to Host1
    NetDeviceContainer router3_host1 = p2p.Install(routers.Get(3), hosts.Get(1));

    // Install IPv6 Internet stack on all nodes
    InternetStackHelper stack;
    stack.Install(allNodes);

    // Assign IPv6 addresses
    Ipv6AddressHelper address;

    // Set base network prefixes
    address.SetBase(Ipv6Address("2001:db8::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer host0_router0_ifaces = address.Assign(host0_router0);
    address.NewNetwork();
    Ipv6InterfaceContainer router0_router1_ifaces = address.Assign(router0_router1);
    address.NewNetwork();
    Ipv6InterfaceContainer router1_router2_ifaces = address.Assign(router1_router2);
    address.NewNetwork();
    Ipv6InterfaceContainer router2_router3_ifaces = address.Assign(router2_router3);
    address.NewNetwork();
    Ipv6InterfaceContainer router3_host1_ifaces = address.Assign(router3_host1);

    // Manually configure routing for loose source routing path: Host0 -> Router0 -> Router1 -> Router2 -> Router3 -> Host1

    // Host0 route to Host1 via Router0
    Ipv6StaticRoutingHelper ipv6RoutingHelper;
    Ptr<Ipv6> host0_ipv6 = hosts.Get(0)->GetObject<Ipv6>();
    Ptr<Ipv6RoutingProtocol> host0_routing = ipv6RoutingHelper.GetStaticRouting(host0_ipv6);
    host0_routing->AddHostRouteTo(Ipv6Address("2001:db8:4::2"), Ipv6Address("2001:db8:0::2"), 1); // Host0 -> Router0

    // Router0 route to Host1 through Router1
    Ptr<Ipv6> router0_ipv6 = routers.Get(0)->GetObject<Ipv6>();
    Ptr<Ipv6RoutingProtocol> router0_routing = ipv6RoutingHelper.GetStaticRouting(router0_ipv6);
    router0_routing->AddHostRouteTo(Ipv6Address("2001:db8:4::2"), Ipv6Address("2001:db8:1::2"), 2); // Router0 -> Router1

    // Router1 route to Host1 through Router2
    Ptr<Ipv6> router1_ipv6 = routers.Get(1)->GetObject<Ipv6>();
    Ptr<Ipv6RoutingProtocol> router1_routing = ipv6RoutingHelper.GetStaticRouting(router1_ipv6);
    router1_routing->AddHostRouteTo(Ipv6Address("2001:db8:4::2"), Ipv6Address("2001:db8:2::2"), 2); // Router1 -> Router2

    // Router2 route to Host1 through Router3
    Ptr<Ipv6> router2_ipv6 = routers.Get(2)->GetObject<Ipv6>();
    Ptr<Ipv6RoutingProtocol> router2_routing = ipv6RoutingHelper.GetStaticRouting(router2_ipv6);
    router2_routing->AddHostRouteTo(Ipv6Address("2001:db8:4::2"), Ipv6Address("2001:db8:3::2"), 2); // Router2 -> Router3

    // Router3 route to Host1 directly
    Ptr<Ipv6> router3_ipv6 = routers.Get(3)->GetObject<Ipv6>();
    Ptr<Ipv6RoutingProtocol> router3_routing = ipv6RoutingHelper.GetStaticRouting(router3_ipv6);
    router3_routing->AddHostRouteTo(Ipv6Address("2001:db8:4::2"), Ipv6Address("2001:db8:4::2"), 2); // Router3 -> Host1

    // Set up ICMPv6 Echo Server on Host1
    uint32_t packetSize = 1024;
    uint32_t maxPacketCount = 5;
    Time interPacketInterval = Seconds(1.0);

    Icmpv6EchoServerHelper echoServer;
    ApplicationContainer serverApp = echoServer.Install(hosts.Get(1));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Set up ICMPv6 Echo Client on Host0
    Icmpv6EchoClientHelper echoClient;
    echoClient.SetRemote(Ipv6Address("2001:db8:4::2")); // Host1's IPv6 address
    echoClient.SetAttribute("MaxPackets", UintegerValue(maxPacketCount));
    echoClient.SetAttribute("Interval", TimeValue(interPacketInterval));
    echoClient.SetAttribute("PacketSize", UintegerValue(packetSize));

    ApplicationContainer clientApp = echoClient.Install(hosts.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Setup tracing
    AsciiTraceHelper asciiTraceHelper;
    p2p.EnableAsciiAll(asciiTraceHelper.CreateFileStream("loose-routing-ipv6.tr"));

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}