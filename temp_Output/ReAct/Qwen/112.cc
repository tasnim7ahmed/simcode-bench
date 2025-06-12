#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/csma-module.h"
#include "ns3/ipv6-routing-table-entry.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("RadvdTwoPrefixSimulation");

int main(int argc, char *argv[]) {
    LogComponentEnable("RadvdTwoPrefixSimulation", LOG_LEVEL_INFO);
    LogComponentEnable("Ipv6L3Protocol", LOG_LEVEL_INFO);
    LogComponentEnable("Icmpv6L4Protocol", LOG_LEVEL_INFO);

    NodeContainer nodes;
    nodes.Create(2);

    NodeContainer router;
    router.Create(1);

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", DataRateValue(DataRate("5Mbps")));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));

    NetDeviceContainer devices0 = csma.Install(NodeContainer(nodes.Get(0), router.Get(0)));
    NetDeviceContainer devices1 = csma.Install(NodeContainer(nodes.Get(1), router.Get(0)));

    InternetStackHelper internet;
    internet.Install(nodes);
    internet.Install(router);

    Ipv6AddressHelper address;

    address.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer interfaces0 = address.Assign(devices0);
    interfaces0.SetForwarding(1, true);
    interfaces0.SetDefaultRouteInfiniteMetric(1, false);

    address.SetBase(Ipv6Address("2001:ABCD::"), Ipv6Prefix(64));
    address.Assign(devices0);

    address.SetBase(Ipv6Address("2001:2::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer interfaces1 = address.Assign(devices1);
    interfaces1.SetForwarding(1, true);
    interfaces1.SetDefaultRouteInfiniteMetric(1, false);

    Ipv6StaticRoutingHelper routingHelper;
    Ptr<Ipv6> ipv6 = router.Get(0)->GetObject<Ipv6>();
    Ptr<Ipv6RoutingProtocol> routing = routingHelper.GetStaticRouting(ipv6);
    routing->PrintRoutingTable(Ptr<OutputStreamWrapper>(Create<OutputStreamWrapper>("radvd-two-prefix-routing-table.txt", std::ios::out)));

    RadvdHelper radvdHelper;

    radvdHelper.AddAnnouncedPrefix(0, Ipv6Address("2001:1::"), 64);
    radvdHelper.AddAnnouncedPrefix(0, Ipv6Address("2001:ABCD::"), 64);
    radvdHelper.SetDefaultLifeTime(0, 600);
    radvdHelper.SetMaxRtrAdvInterval(0, 200);
    radvdHelper.SetMinRtrAdvInterval(0, 100);
    radvdHelper.SetRouterAdvertMessage(0, true);

    radvdHelper.AddAnnouncedPrefix(1, Ipv6Address("2001:2::"), 64);
    radvdHelper.SetDefaultLifeTime(1, 600);
    radvdHelper.SetMaxRtrAdvInterval(1, 200);
    radvdHelper.SetMinRtrAdvInterval(1, 100);
    radvdHelper.SetRouterAdvertMessage(1, true);

    ApplicationContainer radvdApps = radvdHelper.Install(router);
    radvdApps.Start(Seconds(1.0));
    radvdApps.Stop(Seconds(10.0));

    Ping6Helper pingHelper;
    pingHelper.SetLocal(nodes.Get(0)->GetObject<Ipv6>());
    pingHelper.SetRemote(Ipv6Address("2001:2::2"));
    pingHelper.SetAttribute("Count", UintegerValue(5));
    pingHelper.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    pingHelper.SetAttribute("Timeout", TimeValue(Seconds(10.0)));

    ApplicationContainer pingApps = pingHelper.Install(nodes.Get(0));
    pingApps.Start(Seconds(2.0));
    pingApps.Stop(Seconds(7.0));

    AsciiTraceHelper ascii;
    csma.EnableAsciiAll(ascii.CreateFileStream("radvd-two-prefix.tr"));

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}