#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv6-routing-table-entry.h"
#include "ns3/ipv6-list-router.h"
#include "ns3/ipv6-static-routing-helper.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable logging
    LogComponentEnable("Ipv6L3Protocol", LOG_LEVEL_INFO);
    LogComponentEnable("Ping6Application", LOG_LEVEL_INFO);
    LogComponentEnable("Icmpv6L4Protocol", LOG_LEVEL_INFO);
    LogComponentEnable("Ipv6StaticRouting", LOG_LEVEL_INFO);

    // Create nodes
    Ptr<Node> n0 = CreateObject<Node>();
    Ptr<Node> r = CreateObject<Node>();
    Ptr<Node> n1 = CreateObject<Node>();

    NodeContainer net1(n0, r);
    NodeContainer net2(r, n1);
    NodeContainer all(n0, r, n1);

    // Install CSMA devices
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", DataRateValue(DataRate("5Mbps")));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));

    NetDeviceContainer devNet1 = csma.Install(net1);
    NetDeviceContainer devNet2 = csma.Install(net2);

    // Install Internet stack
    InternetStackHelper internet;
    Ipv6ListRoutingHelper listRH;
    Ipv6StaticRoutingHelper staticRh;

    listRH.Add(staticRh, 0);

    internet.SetIpv4StackInstall(false);
    internet.SetIpv6StackInstall(true);
    internet.Install(all);

    // Assign IPv6 addresses
    Ipv6AddressHelper ipv6;

    // Network 1: 2001:1::/64 and 2001:ABCD::/64 on router interface connected to n0
    ipv6.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer ifNet1 = ipv6.Assign(devNet1);
    ifNet1.SetForwarding(1, true);
    ifNet1.SetDefaultRouteInfiniteMetric(1, true);

    // Add second prefix to router's interface towards n0
    ipv6.NewAddress();
    ipv6.Assign(devNet1.Get(1));  // Assign second address from same base or manually:
    ifNet1.Get(1)->AddAddress(1, Ipv6InterfaceAddress(Ipv6Address("2001:ABCD::1"), Ipv6Prefix(64)));

    // Network 2: 2001:2::/64
    ipv6.SetBase(Ipv6Address("2001:2::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer ifNet2 = ipv6.Assign(devNet2);
    ifNet2.SetForwarding(0, true);
    ifNet2.SetDefaultRouteInfiniteMetric(0, true);

    // Configure routing
    Ptr<Ipv6StaticRouting> routerStaticRouting = staticRh.GetStaticRouting(r->GetObject<Ipv6>());
    routerStaticRouting->AddNetworkRouteTo(Ipv6Address("2001:1::"), Ipv6Prefix(64), 1);
    routerStaticRouting->AddNetworkRouteTo(Ipv6Address("2001:ABCD::"), Ipv6Prefix(64), 1);
    routerStaticRouting->AddNetworkRouteTo(Ipv6Address("2001:2::"), Ipv6Prefix(64), 2);

    // Set up Router Advertisements
    Ipv6Address network1Addr1 = ifNet1.GetAddress(1, 1);  // 2001:1::1
    Ipv6Address network1Addr2 = Ipv6Address("2001:ABCD::1");

    // RA for subnet 2001:1::/64 and 2001:ABCD::/64 on interface facing n0
    Simulator::Schedule(Seconds(2.0), &Ipv6L3Protocol::SendRouterAdvert, r->GetObject<Ipv6>(), devNet1.Get(1),
                        Ipv6Address::GetAllNodesMulticast(), 1,
                        std::vector<Ipv6Address>({network1Addr1, network1Addr2}),
                        std::vector<uint8_t>({64, 64}), true, true, 1800, 0, 0, 0, 0);

    // RA for subnet 2001:2::/64 on interface facing n1
    Simulator::Schedule(Seconds(2.0), &Ipv6L3Protocol::SendRouterAdvert, r->GetObject<Ipv6>(), devNet2.Get(0),
                        Ipv6Address::GetAllNodesMulticast(), 1,
                        std::vector<Ipv6Address>({ifNet2.GetAddress(0, 1)}),
                        std::vector<uint8_t>({64}), true, true, 1800, 0, 0, 0, 0);

    // Print assigned IP addresses
    for (uint32_t i = 0; i < all.GetN(); ++i) {
        Ptr<Ipv6> ipv6Node = all.Get(i)->GetObject<Ipv6>();
        std::cout << "Node " << i << " IPv6 addresses:" << std::endl;
        for (uint32_t j = 0; j < ipv6Node->GetNInterfaces(); ++j) {
            for (uint32_t k = 0; k < ipv6Node->GetNAddresses(j); ++k) {
                Ipv6InterfaceAddress addr = ipv6Node->GetAddress(j, k);
                std::cout << " Interface " << j << ": " << addr.GetAddress() << std::endl;
            }
        }
    }

    // Ping application: n0 pings n1
    Ping6Helper ping6;
    ping6.SetLocal(n0, Ipv6Address::GetAny());
    ping6.SetRemote(ifNet2.GetAddress(1, 1));  // Address of n1
    ping6.SetAttribute("MaxPackets", UintegerValue(5));
    ping6.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    ping6.SetAttribute("PacketSize", UintegerValue(64));

    ApplicationContainer apps = ping6.Install(n0);
    apps.Start(Seconds(3.0));
    apps.Stop(Seconds(10.0));

    // Tracing
    AsciiTraceHelper asciiTraceHelper;
    Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream("radvd-two-prefix.tr");
    csma.EnableAsciiAll(stream);

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}