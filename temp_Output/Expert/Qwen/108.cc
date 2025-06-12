#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv6-static-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("Icmpv6RedirectExample");

int main(int argc, char *argv[]) {
    Config::SetDefault("ns3::Ipv4GlobalRouting::RandomEcmpRouting", BooleanValue(false));
    Config::SetDefault("ns3::Ipv4GlobalRouting::UseRestrictedRoute", BooleanValue(false));

    NodeContainer sta1;
    NodeContainer r1;
    NodeContainer r2;
    NodeContainer sta2;

    sta1.Create(1);
    r1.Create(1);
    r2.Create(1);
    sta2.Create(1);

    NodeContainer p2pSta1R1 = NodeContainer(sta1, r1);
    NodeContainer p2pR1R2 = NodeContainer(r1, r2);
    NodeContainer p2pR2Sta2 = NodeContainer(r2, sta2);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer sta1r1Devices = p2p.Install(p2pSta1R1);
    NetDeviceContainer r1r2Devices = p2p.Install(p2pR1R2);
    NetDeviceContainer r2sta2Devices = p2p.Install(p2pR2Sta2);

    InternetStackHelper internet;
    internet.InstallAll();

    Ipv6AddressHelper ipv6;

    ipv6.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer sta1r1Interfaces = ipv6.Assign(sta1r1Devices);

    ipv6.SetBase(Ipv6Address("2001:2::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer r1r2Interfaces = ipv6.Assign(r1r2Devices);

    ipv6.SetBase(Ipv6Address("2001:3::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer r2sta2Interfaces = ipv6.Assign(r2sta2Devices);

    // Assign addresses manually for better control
    sta1r1Interfaces.SetForwarding(0, true);
    sta1r1Interfaces.SetDefaultRouteInfiniteMetric(0, 0);
    sta1r1Interfaces.AddAddress(0, Ipv6InterfaceAddress(Ipv6Address("2001:1::1"), Ipv6Prefix(64)));
    sta1r1Interfaces.SetDown(0);
    sta1r1Interfaces.SetUp(0);

    r1r2Interfaces.SetForwarding(0, true);
    r1r2Interfaces.SetDefaultRouteInfiniteMetric(0, 0);
    r1r2Interfaces.AddAddress(0, Ipv6InterfaceAddress(Ipv6Address("2001:2::1"), Ipv6Prefix(64)));
    r1r2Interfaces.SetDown(0);
    r1r2Interfaces.SetUp(0);

    r1r2Interfaces.SetForwarding(1, true);
    r1r2Interfaces.SetDefaultRouteInfiniteMetric(1, 0);
    r1r2Interfaces.AddAddress(1, Ipv6InterfaceAddress(Ipv6Address("2001:2::2"), Ipv6Prefix(64)));
    r1r2Interfaces.SetDown(1);
    r1r2Interfaces.SetUp(1);

    r2sta2Interfaces.SetForwarding(0, true);
    r2sta2Interfaces.SetDefaultRouteInfiniteMetric(0, 0);
    r2sta2Interfaces.AddAddress(0, Ipv6InterfaceAddress(Ipv6Address("2001:3::1"), Ipv6Prefix(64)));
    r2sta2Interfaces.SetDown(0);
    r2sta2Interfaces.SetUp(0);

    Ipv6InterfaceAddress ifaceAddr;
    ifaceAddr = Ipv6InterfaceAddress(Ipv6Address("2001:1::2"), Ipv6Prefix(64));
    sta1r1Interfaces.GetInterface(0)->AddAddress(ifaceAddr);
    sta1r1Interfaces.SetForwarding(0, false);
    sta1r1Interfaces.SetDefaultRouteInfiniteMetric(0, 1);
    sta1r1Interfaces.SetMetric(0, 1);
    sta1r1Interfaces.SetState(0, true);

    ifaceAddr = Ipv6InterfaceAddress(Ipv6Address("2001:3::2"), Ipv6Prefix(64));
    r2sta2Interfaces.GetInterface(0)->AddAddress(ifaceAddr);
    r2sta2Interfaces.SetForwarding(0, false);
    r2sta2Interfaces.SetDefaultRouteInfiniteMetric(0, 1);
    r2sta2Interfaces.SetMetric(0, 1);
    r2sta2Interfaces.SetState(0, true);

    Ipv6StaticRoutingHelper routingHelper;

    Ptr<Ipv6> sta1Ipv6 = sta1.Get(0)->GetObject<Ipv6>();
    Ptr<Ipv6StaticRouting> sta1Routing = routingHelper.GetStaticRouting(sta1Ipv6);
    sta1Routing->AddNetworkRouteTo(Ipv6Address("2001:3::"), Ipv6Prefix(64), Ipv6Address("2001:1::1"), 1, 0);

    Ptr<Ipv6> r1Ipv6 = r1.Get(0)->GetObject<Ipv6>();
    Ptr<Ipv6StaticRouting> r1Routing = routingHelper.GetStaticRouting(r1Ipv6);
    r1Routing->AddNetworkRouteTo(Ipv6Address("2001:3::"), Ipv6Prefix(64), Ipv6Address("2001:2::2"), 2, 0);

    Ptr<Ipv6> r2Ipv6 = r2.Get(0)->GetObject<Ipv6>();
    Ptr<Ipv6StaticRouting> r2Routing = routingHelper.GetStaticRouting(r2Ipv6);
    r2Routing->AddNetworkRouteTo(Ipv6Address("2001:1::"), Ipv6Prefix(64), Ipv6Address("2001:2::1"), 1, 0);

    V4PingHelper ping6(sta2.Get(0)->GetObject<Ipv6>()->GetAddress(0, 1).GetAddress());
    ping6.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    ping6.SetAttribute("Size", UintegerValue(64));
    ApplicationContainer sta1App = ping6.Install(sta1.Get(0));
    sta1App.Start(Seconds(1.0));
    sta1App.Stop(Seconds(10.0));

    AsciiTraceHelper ascii;
    p2p.EnableAsciiAll(ascii.CreateFileStream("icmpv6-redirect.tr"));

    Simulator::Stop(Seconds(12.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}