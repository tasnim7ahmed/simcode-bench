#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // Enable logging for debugging/tracing
    LogComponentEnable("V6Ping", LOG_LEVEL_INFO);

    // Create nodes: n0, r, n1
    NodeContainer n0r;
    n0r.Add(CreateObject<Node>());
    n0r.Add(CreateObject<Node>()); // router r

    NodeContainer rn1;
    rn1.Add(n0r.Get(1)); // router r
    rn1.Add(CreateObject<Node>());

    Ptr<Node> n0 = n0r.Get(0);
    Ptr<Node> r  = n0r.Get(1);
    Ptr<Node> n1 = rn1.Get(1);

    // Install CSMA devices and channels
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer d0r = csma.Install(n0r);
    NetDeviceContainer dr1 = csma.Install(rn1);

    // Install IPv6 internet stack
    InternetStackHelper internetv6;
    Ipv6StaticRoutingHelper ipv6RoutingHelper;
    internetv6.SetIpv4StackInstall(false);
    internetv6.Install(NodeContainer(n0, r, n1));

    // Assign IPv6 addresses
    Ipv6AddressHelper ipv6addr;
    ipv6addr.SetBase(Ipv6Address("2001:0:1::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer i0r = ipv6addr.Assign(d0r);
    i0r.SetForwarding(1, true); // Enable forwarding on router's iface 0

    ipv6addr.SetBase(Ipv6Address("2001:0:2::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer ir1 = ipv6addr.Assign(dr1);
    ir1.SetForwarding(0, true); // Enable forwarding on router's iface 1

    // Configure router forwarding
    Ptr<Ipv6> ipv6r = r->GetObject<Ipv6>();
    for (uint32_t i = 0; i < ipv6r->GetNInterfaces(); ++i) {
        ipv6r->SetForwarding(i, true);
        ipv6r->SetDefaultRoute(0, i);
    }

    // Set default routes for n0 and n1
    Ptr<Ipv6StaticRouting> staticRoutingN0 = ipv6RoutingHelper.GetStaticRouting(n0->GetObject<Ipv6>());
    staticRoutingN0->SetDefaultRoute(i0r.GetAddress(1, 1), 1);

    Ptr<Ipv6StaticRouting> staticRoutingN1 = ipv6RoutingHelper.GetStaticRouting(n1->GetObject<Ipv6>());
    staticRoutingN1->SetDefaultRoute(ir1.GetAddress(0, 1), 1);

    // Set up ping (V6Ping) application on n0 targeting n1's address
    V6PingHelper pingHelper(ir1.GetAddress(1, 1));
    pingHelper.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    pingHelper.SetAttribute("Verbose", BooleanValue(true));
    ApplicationContainer pingApps = pingHelper.Install(n0);
    pingApps.Start(Seconds(2.0));
    pingApps.Stop(Seconds(10.0));

    // Enable PCAP and ASCII tracing
    csma.EnablePcapAll("csma-ipv6", true, true);
    AsciiTraceHelper ascii;
    csma.EnableAsciiAll(ascii.CreateFileStream("csma-ipv6.tr"));

    // Print IPv6 routing table of n0 at time 1.5s
    Simulator::Schedule(Seconds(1.5), &Ipv6RoutingHelper::PrintRoutingTable, 
                        &ipv6RoutingHelper, n0->GetObject<Node>(), Create<OutputStreamWrapper>(&std::cout));

    Simulator::Run();
    Simulator::Destroy();
    return 0;
}