#include "ns3/core-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv6-routing-table-entry.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("BasicIpv6Network");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("Icmpv6L4Protocol", LOG_LEVEL_INFO);

    NodeContainer nodes;
    nodes.Create(3);
    Ptr<Node> n0 = nodes.Get(0);
    Ptr<Node> r = nodes.Get(1);
    Ptr<Node> n1 = nodes.Get(2);

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", DataRateValue(DataRate("5Mbps")));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));

    NetDeviceContainer dev_n0r = csma.Install(NodeContainer(n0, r));
    NetDeviceContainer dev_rn1 = csma.Install(NodeContainer(r, n1));

    InternetStackHelper internet;
    internet.SetIpv4StackEnabled(false);
    internet.SetIpv6StackEnabled(true);
    internet.Install(nodes);

    Ipv6AddressHelper ipv6;

    ipv6.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer if_n0r = ipv6.Assign(dev_n0r);
    if_n0r.SetForwarding(1, true);
    if_n0r.SetDefaultRouteInfiniteMetric(1, true);

    ipv6.SetBase(Ipv6Address("2001:2::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer if_rn1 = ipv6.Assign(dev_rn1);
    if_rn1.SetForwarding(0, true);
    if_rn1.SetDefaultRouteInfiniteMetric(0, true);

    Ipv6StaticRoutingHelper routingHelper;
    Ptr<Ipv6StaticRouting> routing_r_n0 = routingHelper.GetStaticRouting(r->GetObject<Ipv6>());
    routing_r_n0->AddNetworkRouteTo(Ipv6Address("2001:1::"), Ipv6Prefix(64), 1);

    Ptr<Ipv6StaticRouting> routing_r_n1 = routingHelper.GetStaticRouting(r->GetObject<Ipv6>());
    routing_r_n1->AddNetworkRouteTo(Ipv6Address("2001:2::"), Ipv6Prefix(64), 0);

    Ipv6InterfaceContainer if_n1 = r->GetObject<Ipv6>()->GetInterface(1);
    Ipv6Address n1Addr = if_n1.GetAddress(0, 0);

    V4PingHelper ping(n1Addr.Get());
    ping.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    ping.SetAttribute("Size", UintegerValue(64));
    ApplicationContainer apps = ping.Install(n0);
    apps.Start(Seconds(1.0));
    apps.Stop(Seconds(10.0));

    AsciiTraceHelper ascii;
    csma.EnableAsciiAll(ascii.CreateFileStream("basic-ipv6-network.tr"));
    csma.EnablePcapAll("basic-ipv6-network");

    Ipv6RoutingHelper::PrintIpv6RoutingTableAt(Seconds(2.0), n0, Create<OutputStreamWrapper>("n0-routing-table.txt", std::ios::out));

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}