#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/csma-module.h"
#include "ns3/ipv6-address.h"
#include "ns3/ipv6-routing-helper.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    CommandLine cmd;
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);

    NodeContainer nodes;
    nodes.Create(3);
    NodeContainer n0r = NodeContainer(nodes.Get(0), nodes.Get(1));
    NodeContainer rn1 = NodeContainer(nodes.Get(1), nodes.Get(2));

    CsmaHelper csma01;
    csma01.SetChannelAttribute("DataRate", DataRateValue(DataRate("5Mbps")));
    csma01.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));
    NetDeviceContainer d0d1 = csma01.Install(n0r);

    CsmaHelper csma12;
    csma12.SetChannelAttribute("DataRate", DataRateValue(DataRate("5Mbps")));
    csma12.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));
    NetDeviceContainer d1d2 = csma12.Install(rn1);

    InternetStackHelper internetv6;
    internetv6.Install(nodes);

    Ipv6AddressHelper ipv6;
    ipv6.SetBase(Ipv6Address("2001:db8:0:1::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer i0i1 = ipv6.Assign(d0d1);
    ipv6.SetBase(Ipv6Address("2001:db8:0:2::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer i1i2 = ipv6.Assign(d1d2);

    // Set IPv6 global address
    i0i1.GetAddress(0, 1);
    i0i1.GetAddress(1, 1);
    i1i2.GetAddress(0, 1);
    i1i2.GetAddress(1, 1);

    // Enable IPv6 forwarding on the router
    nodes.Get(1)->GetObject<Ipv6>()->SetForwarding(0, true);

    Ipv6StaticRoutingHelper ipv6RoutingHelper;
    Ptr<Ipv6StaticRouting> staticRouting = ipv6RoutingHelper.GetOrCreateRouting(nodes.Get(0)->GetObject<Ipv6>()->GetRoutingProtocol());
    staticRouting->SetDefaultRoute("2001:db8:0:1::2", 0);
    staticRouting = ipv6RoutingHelper.GetOrCreateRouting(nodes.Get(2)->GetObject<Ipv6>()->GetRoutingProtocol());
    staticRouting->SetDefaultRoute("2001:db8:0:2::1", 1);

    // Ping application
    V6PingHelper ping6(i1i2.GetAddress(1), Seconds(1));
    ping6.SetAttribute("Verbose", BooleanValue(true));
    ApplicationContainer pingApps = ping6.Install(nodes.Get(0));
    pingApps.Start(Seconds(2));
    pingApps.Stop(Seconds(10));

    // Tracing
    AsciiTraceHelper ascii;
    csma01.EnableAsciiAll(ascii.CreateFileStream("ipv6_simulation.tr"));
    csma01.EnablePcapAll("ipv6_simulation", true);
    csma12.EnableAsciiAll(ascii.CreateFileStream("ipv6_simulation.tr"));
    csma12.EnablePcapAll("ipv6_simulation", true);

    //Print Routing Table of Node 0
    Ptr<OutputStreamWrapper> routingStream = Create<OutputStreamWrapper>("ipv6-routing-table.txt", std::ios::out);
    nodes.Get(0)->GetObject<Ipv6>()->GetRoutingProtocol()->PrintRoutingTable(routingStream);

    Simulator::Stop(Seconds(10));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}