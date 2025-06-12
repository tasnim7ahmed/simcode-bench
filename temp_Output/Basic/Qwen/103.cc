#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv6-routing-table-entry.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("Ipv6ThreeNodesWithRouter");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("Ipv6ThreeNodesWithRouter", LOG_LEVEL_INFO);
    LogComponentEnable("Icmpv6L4Protocol", LOG_LEVEL_INFO);

    // Create nodes
    NodeContainer nodes;
    nodes.Create(3);

    Ptr<Node> n0 = nodes.Get(0);
    Ptr<Node> r = nodes.Get(1);
    Ptr<Node> n1 = nodes.Get(2);

    // Install IPv6 Internet Stack on all nodes
    InternetStackHelper internet;
    internet.Install(nodes);

    // Enable IPv6 forwarding on the router
    Ipv6ListRoutingHelper listRH;
    Ipv6StaticRoutingHelper staticRh;
    listRH.Add(staticRh, 0);
    internet.SetRoutingHelper(listRH);
    internet.Install(r);

    // Create CSMA channels
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", DataRateValue(DataRate("5Mbps")));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));

    // Connect nodes through CSMA
    NetDeviceContainer devN0R, devRN1;

    devN0R = csma.Install(NodeContainer(n0, r));
    devRN1 = csma.Install(NodeContainer(r, n1));

    // Assign IPv6 addresses
    Ipv6AddressHelper ipv6;

    ipv6.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer ifN0R = ipv6.Assign(devN0R);

    ipv6.SetBase(Ipv6Address("2001:2::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer ifRN1 = ipv6.Assign(devRN1);

    // Configure default routes
    Ptr<Ipv6> ipv6n0 = n0->GetObject<Ipv6>();
    Ptr<Ipv6RoutingTableEntry> routeN0 = Ipv6RoutingTableEntry::CreateNetworkRouteTo(
        Ipv6Address("2001:2::"), Ipv6Prefix(64), 1);
    Ipv6StaticRoutingHelper::AddRoutingTableEntry(n0, routeN0);

    // Configure default routes on router
    Ipv6StaticRoutingHelper rsh;
    rsh.AddHostRouteTo(Ipv6Address("2001:1::1"), Ipv6Address("2001:2::2"), 1);
    rsh.AddHostRouteTo(Ipv6Address("2001:2::2"), Ipv6Address("2001:1::1"), 0);

    // Enable tracing
    AsciiTraceHelper ascii;
    csma.EnableAsciiAll(ascii.CreateFileStream("ipv6-three-nodes-router.tr"));

    csma.EnablePcapAll("ipv6-three-nodes-router");

    // Set up ping application from n0 to n1
    V4PingHelper ping6(n1->GetObject<Ipv6>()->GetAddress(1, 0).GetAddress(0, 0));
    ping6.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    ping6.SetAttribute("Size", UintegerValue(100));
    ApplicationContainer apps = ping6.Install(n0);
    apps.Start(Seconds(1.0));
    apps.Stop(Seconds(10.0));

    // Print routing table of node n0
    Ipv6StaticRoutingHelper::PrintRoutingTable(n0);

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}