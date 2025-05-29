#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/ipv6-address-helper.h"
#include "ns3/applications-module.h"
#include "ns3/ipv6-routing-helper.h"
#include "ns3/radvd-helper.h"
#include "ns3/ipv6-ping-helper.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // Enable logs
    LogComponentEnable("Ipv6L3Protocol", LOG_LEVEL_INFO);
    LogComponentEnable("Ipv6StaticRouting", LOG_LEVEL_INFO);

    // Create nodes: n0, router, n1
    NodeContainer hosts;
    hosts.Create(2); // n0 (0), n1 (1)
    Ptr<Node> n0 = hosts.Get(0);
    Ptr<Node> n1 = hosts.Get(1);
    Ptr<Node> router = CreateObject<Node>();

    // CSMA channels for each subnet
    NodeContainer subnet1;
    subnet1.Add(n0);
    subnet1.Add(router);

    NodeContainer subnet2;
    subnet2.Add(router);
    subnet2.Add(n1);

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay", StringValue("2ms"));

    // Install devices
    NetDeviceContainer devSubnet1 = csma.Install(subnet1);
    NetDeviceContainer devSubnet2 = csma.Install(subnet2);

    // Install IPv6 internet stack on all nodes
    InternetStackHelper internetv6;
    internetv6.Install(hosts);
    internetv6.Install(router);

    // Assign IPv6 addresses
    Ipv6AddressHelper ipv6_1;
    ipv6_1.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer ifaceSubnet1 = ipv6_1.Assign(devSubnet1);

    Ipv6AddressHelper ipv6_2;
    ipv6_2.SetBase(Ipv6Address("2001:2::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer ifaceSubnet2 = ipv6_2.Assign(devSubnet2);

    // Enable global routing on router
    Ipv6StaticRoutingHelper ipv6RoutingHelper;
    Ptr<Ipv6> ipv6Router = router->GetObject<Ipv6>();
    Ptr<Ipv6StaticRouting> routerStaticRouting = ipv6RoutingHelper.GetStaticRouting(ipv6Router);
    routerStaticRouting->SetDefaultRoute(ipv6Router->GetAddress(1,1).GetAddress(), 2);

    // Set all interfaces up and enable forwarding on router
    for (uint32_t i = 0; i < router->GetNDevices(); ++i)
    {
        ipv6Router->SetForwarding(i, true);
        ipv6Router->SetUp(i);
    }
    Ptr<Ipv6> ipv6n0 = n0->GetObject<Ipv6>();
    Ptr<Ipv6> ipv6n1 = n1->GetObject<Ipv6>();
    for (uint32_t i = 0; i < n0->GetNDevices(); ++i)
        ipv6n0->SetUp(i);
    for (uint32_t i = 0; i < n1->GetNDevices(); ++i)
        ipv6n1->SetUp(i);

    // Configure Radvd for both subnets on router
    RadvdHelper radvd;
    RadvdInterface radvIf1;
    radvIf1.SetInterface(router, 0); // Subnet1 interface
    radvIf1.AddPrefix(Ipv6Address("2001:1::"), 64);

    RadvdInterface radvIf2;
    radvIf2.SetInterface(router, 1); // Subnet2 interface
    radvIf2.AddPrefix(Ipv6Address("2001:2::"), 64);

    radvd.AddRadvdInterface(radvIf1);
    radvd.AddRadvdInterface(radvIf2);

    ApplicationContainer radvdApps = radvd.Install(router);
    radvdApps.Start(Seconds(1.0));
    radvdApps.Stop(Seconds(20.0));

    // Install Ping6 application on n0 to ping n1's address
    Ipv6PingHelper pingHelper(ifaceSubnet2.GetAddress(1,1)); // n1's address on subnet2
    pingHelper.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    pingHelper.SetAttribute("Size", UintegerValue(56));
    pingHelper.SetAttribute("Count", UintegerValue(5));
    ApplicationContainer pingApps = pingHelper.Install(n0);
    pingApps.Start(Seconds(5.0));
    pingApps.Stop(Seconds(18.0));

    // Enable tracing
    AsciiTraceHelper ascii;
    csma.EnableAsciiAll(ascii.CreateFileStream("radvd.tr"));

    // Trace queue events on relevant devices
    Ptr<Queue<Packet>> queue1 = devSubnet1.Get(0)->GetObject<PointToPointNetDevice> () ? 
      devSubnet1.Get(0)->GetObject<PointToPointNetDevice>()->GetQueue() : nullptr;
    Ptr<Queue<Packet>> queue2 = devSubnet2.Get(1)->GetObject<PointToPointNetDevice> () ? 
      devSubnet2.Get(1)->GetObject<PointToPointNetDevice>()->GetQueue() : nullptr;

    // Trace packet receptions on n1
    devSubnet2.Get(1)->TraceConnect("PhyRxEnd", std::string(),
        MakeBoundCallback([](Ptr<const Packet> pkt) {
            std::ofstream out("radvd.tr", std::ios_base::app);
            out << "Packet received at n1 at " << Simulator::Now().GetSeconds() << "s, size=" << pkt->GetSize() << std::endl;
        }));

    // Run simulation
    Simulator::Stop(Seconds(20.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}