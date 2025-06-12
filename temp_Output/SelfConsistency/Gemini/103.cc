#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/ipv6-routing-table.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("Ipv6CsmaExample");

int main(int argc, char* argv[]) {
    bool verbose = false;
    bool tracing = false;

    CommandLine cmd;
    cmd.AddValue("verbose", "Tell application to log if true", verbose);
    cmd.AddValue("tracing", "Enable pcap tracing", tracing);

    cmd.Parse(argc, argv);

    if (verbose) {
        LogComponentEnable("Ipv6CsmaExample", LOG_LEVEL_INFO);
        LogComponentEnable("Ipv6L3Protocol", LOG_LEVEL_ALL);
    }

    // Create nodes
    NodeContainer nodes;
    nodes.Create(2);

    NodeContainer router;
    router.Create(1);

    // Create the point-to-point channel
    CsmaHelper csma01;
    csma01.SetChannelAttribute("DataRate", DataRateValue(DataRate("5Mbps")));
    csma01.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));

    NetDeviceContainer nd0 = csma01.Install(NodeContainer(nodes.Get(0), router.Get(0)));

    CsmaHelper csma11;
    csma11.SetChannelAttribute("DataRate", DataRateValue(DataRate("5Mbps")));
    csma11.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));

    NetDeviceContainer nd1 = csma11.Install(NodeContainer(nodes.Get(1), router.Get(0)));

    // Install IPv6 stack
    InternetStackHelper internet;
    internet.Install(nodes);
    internet.Install(router);

    // Set IPv6 addresses
    Ipv6AddressHelper ipv6;
    ipv6.SetBase(Ipv6Address("2001:db8:1::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer i0 = ipv6.Assign(nd0);

    ipv6.SetBase(Ipv6Address("2001:db8:2::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer i1 = ipv6.Assign(nd1);

    // Configure router interfaces
    Ipv6StaticRoutingHelper ipv6RoutingHelper;
    Ptr<Ipv6StaticRouting> staticRouting = ipv6RoutingHelper.GetOrCreateRouting(router.Get(0)->GetObject<Ipv6>()->GetRoutingProtocol());
    staticRouting->SetDefaultRoute(i0.GetAddress(1), 1);
    staticRouting->AddHostRouteTo(Ipv6Address("2001:db8:1::1"), i0.GetAddress(1), 1);
    staticRouting->AddHostRouteTo(Ipv6Address("2001:db8:2::2"), i1.GetAddress(1), 1);

    staticRouting = ipv6RoutingHelper.GetOrCreateRouting(nodes.Get(0)->GetObject<Ipv6>()->GetRoutingProtocol());
    staticRouting->SetDefaultRoute(i0.GetAddress(1), 1);

    staticRouting = ipv6RoutingHelper.GetOrCreateRouting(nodes.Get(1)->GetObject<Ipv6>()->GetRoutingProtocol());
    staticRouting->SetDefaultRoute(i1.GetAddress(1), 1);

    // Create Applications
    V6PingHelper ping(i1.GetAddress(0));
    ping.SetAttribute("Verbose", BooleanValue(true));
    ApplicationContainer apps = ping.Install(nodes.Get(0));
    apps.Start(Seconds(1.0));
    apps.Stop(Seconds(10.0));

    // Tracing
    if (tracing) {
        csma01.EnablePcap("ipv6-csma", nd0.Get(0), true);
        csma11.EnablePcap("ipv6-csma", nd1.Get(0), true);
    }

    // Animation
    AnimationInterface anim("ipv6-csma.xml");
    anim.SetConstantPosition(nodes.Get(0), 1, 1);
    anim.SetConstantPosition(router.Get(0), 3, 2);
    anim.SetConstantPosition(nodes.Get(1), 5, 1);

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();

    // Print routing table of node n0
    Ptr<Ipv6RoutingTable> routingTable = nodes.Get(0)->GetObject<Ipv6>()->GetRoutingProtocol()->GetRoutingTable();
    routingTable->PrintRoutingTable(std::cout);

    Simulator::Destroy();
    return 0;
}