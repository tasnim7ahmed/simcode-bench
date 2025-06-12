#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv6-address-helper.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/applications-module.h"

using namespace ns3;

int
main(int argc, char *argv[])
{
    CommandLine cmd;
    cmd.Parse(argc, argv);

    NodeContainer nodes;
    nodes.Create(3); // n0, r, n1

    Ptr<Node> n0 = nodes.Get(0);
    Ptr<Node> r = nodes.Get(1);
    Ptr<Node> n1 = nodes.Get(2);

    // CSMA segments: n0--r and r--n1

    NodeContainer csma1Nodes;
    csma1Nodes.Add(n0);
    csma1Nodes.Add(r);

    NodeContainer csma2Nodes;
    csma2Nodes.Add(r);
    csma2Nodes.Add(n1);

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(1)));

    NetDeviceContainer d0r = csma.Install(csma1Nodes);
    NetDeviceContainer dr1 = csma.Install(csma2Nodes);

    InternetStackHelper internetv6;
    internetv6.SetIpv4StackInstall(false); // Only IPv6
    internetv6.Install(nodes);

    // Assign IPv6 addresses
    Ipv6AddressHelper ipv6_1;
    ipv6_1.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer i0r = ipv6_1.Assign(d0r);

    Ipv6AddressHelper ipv6_2;
    ipv6_2.SetBase(Ipv6Address("2001:2::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer ir1 = ipv6_2.Assign(dr1);

    // Enable forwarding on router
    r->GetObject<Ipv6>()->SetAttribute("IpForward", BooleanValue(true));

    // Set default routes for n0 and n1
    Ipv6StaticRoutingHelper staticRoutingHelper;

    Ptr<Ipv6StaticRouting> staticRouting_n0 = staticRoutingHelper.GetStaticRouting(n0->GetObject<Ipv6>());
    staticRouting_n0->SetDefaultRoute(i0r.GetAddress(1, 1), 1);

    Ptr<Ipv6StaticRouting> staticRouting_n1 = staticRoutingHelper.GetStaticRouting(n1->GetObject<Ipv6>());
    staticRouting_n1->SetDefaultRoute(ir1.GetAddress(0, 1), 1);

    // (Router static routing: not needed, as interfaces are directly connected.)

    // Disable DAD (Duplicate Address Detection) and set interfaces up
    for (uint32_t i = 0; i < i0r.GetN(); ++i)
    {
        i0r.SetForwarding(i, true);
        i0r.SetDefaultRouteInAllNodes(i);
        Ptr<Ipv6> ipv6 = csma1Nodes.Get(i)->GetObject<Ipv6>();
        ipv6->SetMetric(i0r.GetInterfaceIndex(i), 1);
        ipv6->SetAttribute("UseDefaultRoute", BooleanValue(true));
        i0r.GetInterface(i)->GetDevice()->SetAttribute("Address", Mac48AddressValue(Mac48Address::Allocate()));
        i0r.GetInterface(i)->SetUp();
        i0r.GetInterface(i)->SetNDiscCacheTimeout(Seconds(0));
    }
    for (uint32_t i = 0; i < ir1.GetN(); ++i)
    {
        ir1.SetForwarding(i, true);
        ir1.SetDefaultRouteInAllNodes(i);
        Ptr<Ipv6> ipv6 = csma2Nodes.Get(i)->GetObject<Ipv6>();
        ipv6->SetMetric(ir1.GetInterfaceIndex(i), 1);
        ipv6->SetAttribute("UseDefaultRoute", BooleanValue(true));
        ir1.GetInterface(i)->GetDevice()->SetAttribute("Address", Mac48AddressValue(Mac48Address::Allocate()));
        ir1.GetInterface(i)->SetUp();
        ir1.GetInterface(i)->SetNDiscCacheTimeout(Seconds(0));
    }

    // ICMPv6 Echo Request: Ping6 from n0 to n1
    uint16_t echoPort = 9;
    Ipv6Address remoteAddr = ir1.GetAddress(1, 1); // n1's IPv6 address

    V6PingHelper ping6(remoteAddr);
    ping6.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    ping6.SetAttribute("Verbose", BooleanValue(true));
    ping6.SetAttribute("PacketSize", UintegerValue(56));
    ApplicationContainer pingApps = ping6.Install(n0);
    pingApps.Start(Seconds(2.0));
    pingApps.Stop(Seconds(10.0));

    // ASCII and PCAP tracing
    AsciiTraceHelper ascii;
    csma.EnableAsciiAll(ascii.CreateFileStream("csma6.tr"));
    csma.EnablePcapAll("csma6", true, true);

    // Print IPv6 routing table of n0
    Simulator::Schedule(Seconds(1.5), &Ipv6StaticRoutingHelper::PrintRoutingTable, &staticRoutingHelper, n0, Create<OutputStreamWrapper>(&std::cout));

    Simulator::Stop(Seconds(12.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}