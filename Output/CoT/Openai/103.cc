#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/applications-module.h"

using namespace ns3;

int
main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);

    NodeContainer n0r;
    n0r.Create(2); // n0 and r

    NodeContainer rn1;
    rn1.Add(n0r.Get(1)); // r
    rn1.Create(1);       // n1

    Ptr<Node> n0 = n0r.Get(0);
    Ptr<Node> r = n0r.Get(1);
    Ptr<Node> n1 = rn1.Get(1);

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer d_n0r = csma.Install(n0r);
    NetDeviceContainer d_rn1 = csma.Install(rn1);

    InternetStackHelper stack;
    stack.Install(n0);
    stack.Install(r);
    stack.Install(n1);

    Ipv6AddressHelper ipv6;
    Ipv6InterfaceContainer if_n0r, if_rn1;

    ipv6.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
    if_n0r = ipv6.Assign(d_n0r);
    if_n0r.SetForwarding(1, true);
    if_n0r.SetDefaultRouteInAllNodes(1);

    ipv6.SetBase(Ipv6Address("2001:2::"), Ipv6Prefix(64));
    if_rn1 = ipv6.Assign(d_rn1);
    if_rn1.SetForwarding(0, true);
    if_rn1.SetDefaultRouteInAllNodes(0);

    // Remove default routes set by Assign on r, set manually
    Ptr<Ipv6> ipv6_r = r->GetObject<Ipv6>();
    Ipv6StaticRoutingHelper ipv6RoutingHelper;
    Ptr<Ipv6StaticRouting> staticRoutingR = ipv6RoutingHelper.GetStaticRouting(ipv6_r);
    staticRoutingR->SetDefaultRoute(Ipv6Address("::"), 1, 0);

    // Set default route on n0 to router (interface 1 of n0r)
    Ptr<Ipv6> ipv6_n0 = n0->GetObject<Ipv6>();
    Ptr<Ipv6StaticRouting> staticRoutingN0 = ipv6RoutingHelper.GetStaticRouting(ipv6_n0);
    staticRoutingN0->SetDefaultRoute(if_n0r.GetAddress(1,1), 1, 0);

    // Set default route on n1 to router (interface 0 of rn1)
    Ptr<Ipv6> ipv6_n1 = n1->GetObject<Ipv6>();
    Ptr<Ipv6StaticRouting> staticRoutingN1 = ipv6RoutingHelper.GetStaticRouting(ipv6_n1);
    staticRoutingN1->SetDefaultRoute(if_rn1.GetAddress(0,1), 1, 0);

    // Ping6 application from n0 to n1
    uint32_t packetSize = 56;
    uint32_t maxPackets = 5;
    Time interPacketInterval = Seconds(1.0);

    Ping6Helper ping6;
    ping6.SetLocal(if_n0r.GetAddress(0,1));
    ping6.SetRemote(if_rn1.GetAddress(1,1));
    ping6.SetAttribute("MaxPackets", UintegerValue(maxPackets));
    ping6.SetAttribute("Interval", TimeValue(interPacketInterval));
    ping6.SetAttribute("PacketSize", UintegerValue(packetSize));
    ApplicationContainer apps = ping6.Install(n0);
    apps.Start(Seconds(1.0));
    apps.Stop(Seconds(10.0));

    // Enable ascii and pcap tracing
    AsciiTraceHelper ascii;
    csma.EnableAsciiAll(ascii.CreateFileStream("csma-ipv6.tr"));
    csma.EnablePcapAll("csma-ipv6", true);

    Simulator::Schedule(Seconds(0.5), &Ipv6StaticRoutingHelper::PrintRoutingTable, &ipv6RoutingHelper, n0, std::cout);

    Simulator::Stop(Seconds(11.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}