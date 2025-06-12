#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/ipv6-routing-table-entry.h"
#include "ns3/icmpv6-echo.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    CommandLine cmd;
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("Icmpv6L4Protocol", LOG_LEVEL_INFO);
    LogComponentEnable("Icmpv6EchoClient", LOG_LEVEL_INFO);
    LogComponentEnable("Icmpv6EchoServer", LOG_LEVEL_INFO);

    NodeContainer nodes;
    nodes.Create(6); // Host 0, Host 1, Router 0, Router 1, Router 2, Router 3

    NodeContainer host0Router0 = NodeContainer(nodes.Get(0), nodes.Get(2));
    NodeContainer host1Router3 = NodeContainer(nodes.Get(1), nodes.Get(5));
    NodeContainer router0Router1 = NodeContainer(nodes.Get(2), nodes.Get(3));
    NodeContainer router0Router2 = NodeContainer(nodes.Get(2), nodes.Get(4));
    NodeContainer router1Router3 = NodeContainer(nodes.Get(3), nodes.Get(5));
    NodeContainer router2Router3 = NodeContainer(nodes.Get(4), nodes.Get(5));

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devHost0Router0 = p2p.Install(host0Router0);
    NetDeviceContainer devHost1Router3 = p2p.Install(host1Router3);
    NetDeviceContainer devRouter0Router1 = p2p.Install(router0Router1);
    NetDeviceContainer devRouter0Router2 = p2p.Install(router0Router2);
    NetDeviceContainer devRouter1Router3 = p2p.Install(router1Router3);
    NetDeviceContainer devRouter2Router3 = p2p.Install(router2Router3);

    InternetStackHelper internetv6;
    internetv6.Install(nodes);

    Ipv6AddressHelper ipv6;
    ipv6.SetBase(Ipv6Address("2001:db8:0:1::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer ifHost0Router0 = ipv6.Assign(devHost0Router0);

    ipv6.SetBase(Ipv6Address("2001:db8:0:2::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer ifHost1Router3 = ipv6.Assign(devHost1Router3);

    ipv6.SetBase(Ipv6Address("2001:db8:0:3::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer ifRouter0Router1 = ipv6.Assign(devRouter0Router1);

    ipv6.SetBase(Ipv6Address("2001:db8:0:4::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer ifRouter0Router2 = ipv6.Assign(devRouter0Router2);

    ipv6.SetBase(Ipv6Address("2001:db8:0:5::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer ifRouter1Router3 = ipv6.Assign(devRouter1Router3);

    ipv6.SetBase(Ipv6Address("2001:db8:0:6::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer ifRouter2Router3 = ipv6.Assign(devRouter2Router3);

    // Set up static routes
    Ipv6StaticRoutingHelper ipv6RoutingHelper;
    Ptr<Ipv6StaticRouting> staticRouting;

    // Router 0
    staticRouting = ipv6RoutingHelper.GetOrCreateRouting(nodes.Get(2)->GetObject<Ipv6>());
    staticRouting->AddHostRouteToNetif(Ipv6Address("2001:db8:0:1::1"), 0, ifHost0Router0.GetNetif(1));
    staticRouting->AddNetworkRouteToNetif(Ipv6Address("2001:db8:0:2::"), 64, ifRouter0Router1.GetNetif(1));

    // Router 1
    staticRouting = ipv6RoutingHelper.GetOrCreateRouting(nodes.Get(3)->GetObject<Ipv6>());
    staticRouting->AddNetworkRouteToNetif(Ipv6Address("2001:db8:0:1::"), 64, ifRouter0Router1.GetNetif(0));
    staticRouting->AddHostRouteToNetif(Ipv6Address("2001:db8:0:2::1"), 1, ifRouter1Router3.GetNetif(0));

    // Router 2
    staticRouting = ipv6RoutingHelper.GetOrCreateRouting(nodes.Get(4)->GetObject<Ipv6>());
    staticRouting->AddNetworkRouteToNetif(Ipv6Address("2001:db8:0:1::"), 64, ifRouter0Router2.GetNetif(0));
    staticRouting->AddHostRouteToNetif(Ipv6Address("2001:db8:0:2::1"), 1, ifRouter2Router3.GetNetif(0));

    // Router 3
    staticRouting = ipv6RoutingHelper.GetOrCreateRouting(nodes.Get(5)->GetObject<Ipv6>());
    staticRouting->AddNetworkRouteToNetif(Ipv6Address("2001:db8:0:1::"), 64, ifRouter1Router3.GetNetif(1));
    staticRouting->AddHostRouteToNetif(Ipv6Address("2001:db8:0:2::1"), 0, ifHost1Router3.GetNetif(1));

    // Host 0
    staticRouting = ipv6RoutingHelper.GetOrCreateRouting(nodes.Get(0)->GetObject<Ipv6>());
    staticRouting->AddDefaultRouteToNetif(ifHost0Router0.GetAddress(1), 0, 0);

    // Host 1
    staticRouting = ipv6RoutingHelper.GetOrCreateRouting(nodes.Get(1)->GetObject<Ipv6>());
    staticRouting->AddDefaultRouteToNetif(ifHost1Router3.GetAddress(1), 0, 0);

    // Configure ICMPv6 echo application
    Icmpv6EchoClientHelper echoClientHelper;
    echoClientHelper.SetRemote(ifHost1Router3.GetAddress(0), 1); // Send to Host 1

    ApplicationContainer clientApp = echoClientHelper.Install(nodes.Get(0));
    clientApp.Start(Seconds(1.0));
    clientApp.Stop(Seconds(10.0));

    Icmpv6EchoServerHelper echoServerHelper;
    ApplicationContainer serverApp = echoServerHelper.Install(nodes.Get(1));
    serverApp.Start(Seconds(0.5));
    serverApp.Stop(Seconds(10.0));

    // Enable IPv6 forwarding on routers
    for (int i = 2; i <= 5; ++i) {
        nodes.Get(i)->GetObject<Ipv6>()->SetForwarding(0, true);
    }

    // Add loose source routing option to ICMPv6 echo packets
    Ptr<Icmpv6EchoClient> echoClient = clientApp.Get(0)->GetObject<Icmpv6EchoClient>();
    std::vector<Ipv6Address> routerAddresses;
    routerAddresses.push_back(ifRouter0Router1.GetAddress(1));
    routerAddresses.push_back(ifRouter1Router3.GetAddress(0));
    echoClient->SetLooseSourceRouting(routerAddresses);

    // Enable tracing
    p2p.EnablePcapAll("loose-routing-ipv6");

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}