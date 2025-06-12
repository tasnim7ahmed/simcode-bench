#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv6-routing-table-entry.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("Ipv6RoutingBasicSim");

int main(int argc, char *argv[])
{
    // Enable logging
    LogComponentEnable("Ipv6RoutingBasicSim", LOG_LEVEL_INFO);
    LogComponentEnable("Icmpv6L4Protocol", LOG_LEVEL_ALL);

    // Create nodes
    NodeContainer nodes;
    nodes.Create(3);

    Ptr<Node> n0 = nodes.Get(0);
    Ptr<Node> r = nodes.Get(1);
    Ptr<Node> n1 = nodes.Get(2);

    // Setup CSMA channels
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", DataRateValue(DataRate("5Mbps")));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));

    // Create devices and install them
    NetDeviceContainer ndc0r, ndcr1;

    ndc0r = csma.Install(NodeContainer(n0, r));
    ndcr1 = csma.Install(NodeContainer(r, n1));

    // Install Internet stack
    InternetStackHelper internet;
    internet.Install(nodes);

    // Assign IPv6 addresses
    Ipv6AddressHelper ipv6;

    ipv6.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer iic0r = ipv6.Assign(ndc0r);
    iic0r.SetForwarding(1, true);  // Enable forwarding on router interface
    iic0r.SetDefaultRouteInAllNodes();

    ipv6.SetBase(Ipv6Address("2001:2::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer iicr1 = ipv6.Assign(ndcr1);
    iicr1.SetForwarding(0, true);  // Enable forwarding on router interface

    // Manually set up default routes for the router if needed
    Ipv6StaticRoutingHelper routingHelper;
    Ptr<Ipv6StaticRouting> rtrRouting = routingHelper.GetStaticRouting(r->GetObject<Ipv6>());
    rtrRouting->AddNetworkRouteTo(Ipv6Address("2001:1::"), Ipv6Prefix(64), 1);
    rtrRouting->AddNetworkRouteTo(Ipv6Address("2001:2::"), Ipv6Prefix(64), 2);

    // Print routing table of node n0
    Ipv6RoutingHelper::PrintRoutingTable(n0, Seconds(0.0), Seconds(10.0), "n0.routingtable");

    // Setup ping application (ICMPv6 echo request)
    V4PingHelper ping(n1->GetObject<Ipv6>()->GetAddress(1, 1).GetAddress(0, 0));
    ping.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    ping.SetAttribute("Size", UintegerValue(100));
    ApplicationContainer apps = ping.Install(n0);
    apps.Start(Seconds(1.0));
    apps.Stop(Seconds(10.0));

    // Tracing
    AsciiTraceHelper ascii;
    csma.EnableAsciiAll(ascii.CreateFileStream("ipv6-routing-basic-sim.tr"));
    csma.EnablePcapAll("ipv6-routing-basic-sim");

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}