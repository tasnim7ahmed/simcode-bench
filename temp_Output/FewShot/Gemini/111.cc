#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/internet-apps-module.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/ipv6-routing-table-entry.h"
#include "ns3/csma-module.h"
#include "ns3/radvd-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    CommandLine cmd;
    cmd.Parse(argc, argv);

    // Enable IPv6
    GlobalValue::Bind("Ipv4L3Protocol::Tcp::DefaultRcvBufsize", UintegerValue(65535));
    GlobalValue::Bind("Ipv4L3Protocol::Tcp::DefaultSendBufsize", UintegerValue(65535));

    // Create Nodes
    NodeContainer nodes;
    nodes.Create(2);

    NodeContainer router;
    router.Create(1);

    // Create CSMA channels
    CsmaHelper csma01;
    csma01.SetChannelAttribute("DataRate", StringValue("10Mbps"));
    csma01.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));

    NodeContainer subnet0Nodes;
    subnet0Nodes.Add(nodes.Get(0));
    subnet0Nodes.Add(router.Get(0));

    NetDeviceContainer subnet0Devices = csma01.Install(subnet0Nodes);

    CsmaHelper csma12;
    csma12.SetChannelAttribute("DataRate", StringValue("10Mbps"));
    csma12.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));

    NodeContainer subnet1Nodes;
    subnet1Nodes.Add(nodes.Get(1));
    subnet1Nodes.Add(router.Get(0));

    NetDeviceContainer subnet1Devices = csma12.Install(subnet1Nodes);

    // Install IPv6 stack
    InternetStackHelper internetv6;
    internetv6.Install(nodes);
    internetv6.Install(router);

    // Assign IPv6 Addresses
    Ipv6AddressHelper ipv6;

    ipv6.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer interfaces0 = ipv6.Assign(subnet0Devices);

    ipv6.SetBase(Ipv6Address("2001:2::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer interfaces1 = ipv6.Assign(subnet1Devices);

    // Set IPv6 global addresses
    interfaces0.SetForwarding(0, true);
    interfaces0.SetForwarding(1, true);
    interfaces1.SetForwarding(0, true);
    interfaces1.SetForwarding(1, true);

    // Configure Radvd on the router
    RadvdHelper radvd;
    radvd.SetPrefix(Ipv6Prefix("2001:1::/64"), 0);
    radvd.SetPrefix(Ipv6Prefix("2001:2::/64"), 1);
    ApplicationContainer radvdApps = radvd.Install(router.Get(0));
    radvdApps.Start(Seconds(0.0));
    radvdApps.Stop(Seconds(10.0));

    // Configure static routes manually on the router for packet forwarding
    Ptr<Ipv6StaticRouting> staticRouting = Ipv6RoutingHelper::GetRouting(router.Get(0)->GetObject<Ipv6>());
    staticRouting->SetDefaultRoute(interfaces0.GetAddress(0,1), 0);
    staticRouting = Ipv6RoutingHelper::GetRouting(router.Get(0)->GetObject<Ipv6>());
    staticRouting->SetDefaultRoute(interfaces1.GetAddress(0,1), 1);

    // Ping application
    V6PingHelper ping6(interfaces1.GetAddress(1), 1);
    ping6.SetAttribute("Verbose", BooleanValue(true));
    ApplicationContainer pings = ping6.Install(nodes.Get(0));
    pings.Start(Seconds(1.0));
    pings.Stop(Seconds(10.0));

    // Tracing
    CsmaHelper csma;
    csma.EnablePcapAll("radvd", false);
    CsmaHelper csma2;
    csma2.EnablePcapAll("radvd", false);

    // AnimationInterface anim("radvd.xml");

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}