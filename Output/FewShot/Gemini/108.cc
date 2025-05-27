#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/ipv6-routing-table-entry.h"
#include "ns3/icmpv6-l4-protocol.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    CommandLine cmd;
    cmd.Parse(argc, argv);

    // Create nodes
    NodeContainer staNodes, routerNodes;
    staNodes.Create(2);
    routerNodes.Create(2);

    NodeContainer allNodes;
    allNodes.Add(staNodes);
    allNodes.Add(routerNodes);

    // Create channels
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("1ms"));

    NetDeviceContainer sta1R1Devs = p2p.Install(staNodes.Get(0), routerNodes.Get(0));
    NetDeviceContainer r1R2Devs = p2p.Install(routerNodes.Get(0), routerNodes.Get(1));
    NetDeviceContainer sta2R2Devs = p2p.Install(staNodes.Get(1), routerNodes.Get(1));

    // Install IPv6 stack
    InternetStackHelper internetv6;
    internetv6.Install(allNodes);

    // Assign IPv6 addresses
    Ipv6AddressHelper ipv6;
    ipv6.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer sta1R1Ifaces = ipv6.Assign(sta1R1Devs);

    ipv6.SetBase(Ipv6Address("2001:2::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer r1R2Ifaces = ipv6.Assign(r1R2Devs);

    ipv6.SetBase(Ipv6Address("2001:3::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer sta2R2Ifaces = ipv6.Assign(sta2R2Devs);

    // Configure routing
    Ptr<Ipv6StaticRouting> sta1Routing = Ipv6RoutingHelper::GetRouting <Ipv6StaticRouting> (staNodes.Get(0)->GetObject<Ipv6>()->GetRoutingProtocol());
    sta1Routing->SetDefaultRoute(sta1R1Ifaces.GetAddress(1, 0), 0, sta1R1Ifaces.GetInterface(0));

    Ptr<Ipv6StaticRouting> r1Routing = Ipv6RoutingHelper::GetRouting <Ipv6StaticRouting> (routerNodes.Get(0)->GetObject<Ipv6>()->GetRoutingProtocol());
    r1Routing->AddRouteToNetwork(Ipv6Address("2001:3::"), Ipv6Prefix(64), r1R2Ifaces.GetAddress(1, 0), r1R2Ifaces.GetInterface(0));

    Ptr<Ipv6StaticRouting> sta2Routing = Ipv6RoutingHelper::GetRouting <Ipv6StaticRouting> (staNodes.Get(1)->GetObject<Ipv6>()->GetRoutingProtocol());
    sta2Routing->SetDefaultRoute(sta2R2Ifaces.GetAddress(1, 0), 0, sta2R2Ifaces.GetInterface(0));


    // Set up ping application
    V6PingHelper ping(sta2R2Ifaces.GetAddress(0));
    ping.SetAttribute("Verbose", BooleanValue(true));
    ApplicationContainer pingApp = ping.Install(staNodes.Get(0));
    pingApp.Start(Seconds(1.0));
    pingApp.Stop(Seconds(10.0));

    // Enable Tracing
    p2p.EnablePcapAll("icmpv6-redirect");

    // Run simulation
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}