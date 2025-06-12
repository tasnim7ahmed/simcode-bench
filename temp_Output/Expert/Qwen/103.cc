#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv6-routing-table-entry.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("Ipv6RoutingSimulation");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("Ipv6RoutingSimulation", LOG_LEVEL_INFO);
    LogComponentEnable("Icmpv6L4Protocol", LOG_LEVEL_INFO);

    NodeContainer nodes;
    nodes.Create(3);

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", DataRateValue(DataRate("100Mbps")));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));

    NetDeviceContainer devices0r = csma.Install(NodeContainer(nodes.Get(0), nodes.Get(1)));
    NetDeviceContainer devices1r = csma.Install(NodeContainer(nodes.Get(2), nodes.Get(1)));

    InternetStackHelper stack;
    stack.SetIpv4StackEnabled(false);
    stack.SetIpv6StackEnabled(true);
    stack.Install(nodes);

    Ipv6AddressHelper address;

    address.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer interfaces0r = address.Assign(devices0r);
    interfaces0r.SetForwarding(1, true);
    interfaces0r.SetDefaultRouteInAllNodes(1);

    address.SetBase(Ipv6Address("2001:2::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer interfaces1r = address.Assign(devices1r);
    interfaces1r.SetForwarding(1, true);
    interfaces1r.SetDefaultRouteInAllNodes(1);

    Ipv6StaticRoutingHelper routingHelper;
    Ptr<Ipv6> ipv6_r = nodes.Get(1)->GetObject<Ipv6>();
    Ptr<Ipv6StaticRouting> routing_r = routingHelper.GetStaticRouting(ipv6_r);
    routing_r->AddNetworkRouteTo("2001:1::", Ipv6Prefix(64), 1);
    routing_r->AddNetworkRouteTo("2001:2::", Ipv6Prefix(64), 2);

    V4PingHelper ping(interfaces1r.GetAddress(1, 1));
    ping.SetAttribute("Remote", Ipv6AddressValue(interfaces1r.GetAddress(1, 1)));
    ApplicationContainer apps = ping.Install(nodes.Get(0));
    apps.Start(Seconds(1.0));
    apps.Stop(Seconds(10.0));

    AsciiTraceHelper ascii;
    csma.EnableAsciiAll(ascii.CreateFileStream("ipv6-routing-simulation.tr"));
    csma.EnablePcapAll("ipv6-routing-simulation");

    Ipv6RoutingTableEntry::PrintRoutingTableAt(Seconds(2), nodes.Get(0), ascii.CreateFileStream("n0-ipv6-routing-table.txt"));

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}