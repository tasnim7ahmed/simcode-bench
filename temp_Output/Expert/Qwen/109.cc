#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv6-routing-table-entry.h"
#include "ns3/ipv6-static-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("LooseRoutingIpv6");

int main(int argc, char *argv[]) {
    Config::SetDefault("ns3::Ipv6::IpForward", BooleanValue(true));
    Config::SetDefault("ns3::Ipv6::SendIcmpv6Redirect", BooleanValue(false));

    NodeContainer hosts;
    hosts.Create(2);

    NodeContainer routers;
    routers.Create(4);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer host0Router0 = p2p.Install(hosts.Get(0), routers.Get(0));
    NetDeviceContainer router0Router1 = p2p.Install(routers.Get(0), routers.Get(1));
    NetDeviceContainer router1Router2 = p2p.Install(routers.Get(1), routers.Get(2));
    NetDeviceContainer router2Router3 = p2p.Install(routers.Get(2), routers.Get(3));
    NetDeviceContainer router3Host1 = p2p.Install(routers.Get(3), hosts.Get(1));

    InternetStackHelper internet;
    internet.InstallAll();

    Ipv6AddressHelper ipv6;

    ipv6.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer host0Router0Interfaces = ipv6.Assign(host0Router0);
    host0Router0Interfaces.SetForwarding(0, true);
    host0Router0Interfaces.SetDefaultRouteInfiniteMetric(0, true);

    ipv6.SetBase(Ipv6Address("2001:2::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer router0Router1Interfaces = ipv6.Assign(router0Router1);
    router0Router1Interfaces.SetForwarding(0, true);
    router0Router1Interfaces.SetForwarding(1, true);

    ipv6.SetBase(Ipv6Address("2001:3::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer router1Router2Interfaces = ipv6.Assign(router1Router2);
    router1Router2Interfaces.SetForwarding(0, true);
    router1Router2Interfaces.SetForwarding(1, true);

    ipv6.SetBase(Ipv6Address("2001:4::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer router2Router3Interfaces = ipv6.Assign(router2Router3);
    router2Router3Interfaces.SetForwarding(0, true);
    router2Router3Interfaces.SetForwarding(1, true);

    ipv6.SetBase(Ipv6Address("2001:5::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer router3Host1Interfaces = ipv6.Assign(router3Host1);
    router3Host1Interfaces.SetForwarding(1, true);
    router3Host1Interfaces.SetDefaultRouteInfiniteMetric(1, true);

    Ipv6StaticRoutingHelper routingHelper;

    Ptr<Ipv6StaticRouting> host0Routing = routingHelper.GetStaticRouting(hosts.Get(0)->GetObject<Ipv6>());
    host0Routing->AddHostRouteTo(Ipv6Address("2001:5::2"), Ipv6Address("2001:1::2"), 1);

    Ptr<Ipv6StaticRouting> router0Routing = routingHelper.GetStaticRouting(routers.Get(0)->GetObject<Ipv6>());
    router0Routing->AddHostRouteTo(Ipv6Address("2001:5::2"), Ipv6Address("2001:2::2"), 2);

    Ptr<Ipv6StaticRouting> router1Routing = routingHelper.GetStaticRouting(routers.Get(1)->GetObject<Ipv6>());
    router1Routing->AddHostRouteTo(Ipv6Address("2001:5::2"), Ipv6Address("2001:3::2"), 2);

    Ptr<Ipv6StaticRouting> router2Routing = routingHelper.GetStaticRouting(routers.Get(2)->GetObject<Ipv6>());
    router2Routing->AddHostRouteTo(Ipv6Address("2001:5::2"), Ipv6Address("2001:4::2"), 2);

    Ptr<Ipv6StaticRouting> router3Routing = routingHelper.GetStaticRouting(routers.Get(3)->GetObject<Ipv6>());
    router3Routing->AddNetworkRouteTo(Ipv6Address("2001:1::"), Ipv6Prefix(64), Ipv6Address("2001:4::1"), 1);

    uint16_t echoPort = 9;
    uint32_t packetSize = 1024;
    uint32_t maxPacketCount = 5;
    Time interPacketInterval = Seconds(1.0);

    UdpEchoServerHelper echoServer(echoPort);
    ApplicationContainer serverApps = echoServer.Install(hosts.Get(1));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient(host3Host1Interfaces.GetAddress(1, 1), echoPort);
    echoClient.SetAttribute("MaxPackets", UintegerValue(maxPacketCount));
    echoClient.SetAttribute("Interval", TimeValue(interPacketInterval));
    echoClient.SetAttribute("PacketSize", UintegerValue(packetSize));

    ApplicationContainer clientApps = echoClient.Install(hosts.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    AsciiTraceHelper ascii;
    p2p.EnableAsciiAll(ascii.CreateFileStream("loose-routing-ipv6.tr"));

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}