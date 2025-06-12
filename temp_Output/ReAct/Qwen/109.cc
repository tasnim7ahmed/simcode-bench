#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv6-routing-table-entry.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("LooseRoutingIpv6");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("Icmpv6L4Protocol", LOG_LEVEL_INFO);

    NodeContainer hosts;
    hosts.Create(2);

    NodeContainer routers;
    routers.Create(4);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", DataRateValue(DataRate("1Mbps")));
    p2p.SetChannelAttribute("Delay", TimeValue(MilliSeconds(10)));

    NetDeviceContainer host0_router0 = p2p.Install(hosts.Get(0), routers.Get(0));
    NetDeviceContainer router0_router1 = p2p.Install(routers.Get(0), routers.Get(1));
    NetDeviceContainer router1_router2 = p2p.Install(routers.Get(1), routers.Get(2));
    NetDeviceContainer router2_router3 = p2p.Install(routers.Get(2), routers.Get(3));
    NetDeviceContainer router3_host1 = p2p.Install(routers.Get(3), hosts.Get(1));

    InternetStackHelper internetv6;
    internetv6.SetIpv4StackEnabled(false);
    internetv6.SetIpv6StackEnabled(true);
    internetv6.InstallAll();

    Ipv6AddressHelper ipv6;
    ipv6.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer host0_router0_ifaces = ipv6.Assign(host0_router0);
    host0_router0_ifaces.SetForwarding(0, true);
    host0_router0_ifaces.SetDefaultRouteInAllNodes(0);

    ipv6.NewNetwork();
    Ipv6InterfaceContainer router0_router1_ifaces = ipv6.Assign(router0_router1);
    router0_router1_ifaces.SetForwarding(0, true);
    router0_router1_ifaces.SetForwarding(1, true);

    ipv6.NewNetwork();
    Ipv6InterfaceContainer router1_router2_ifaces = ipv6.Assign(router1_router2);
    router1_router2_ifaces.SetForwarding(0, true);
    router1_router2_ifaces.SetForwarding(1, true);

    ipv6.NewNetwork();
    Ipv6InterfaceContainer router2_router3_ifaces = ipv6.Assign(router2_router3);
    router2_router3_ifaces.SetForwarding(0, true);
    router2_router3_ifaces.SetForwarding(1, true);

    ipv6.NewNetwork();
    Ipv6InterfaceContainer router3_host1_ifaces = ipv6.Assign(router3_host1);
    router3_host1_ifaces.SetForwarding(0, true);
    router3_host1_ifaces.SetDefaultRouteInAllNodes(1);

    Ipv6Address targetAddr = router3_host1_ifaces.GetAddress(1, 1);

    Ipv6RoutingTableEntry routeToHost1 = Ipv6RoutingTableEntry::CreateHostRouteTo(
        targetAddr,
        router3_host1_ifaces.GetAddress(0, 0),
        1
    );
    Ipv6::GetRoutingProtocol(hosts.Get(0))->AddRoute(routeToHost1);

    AsciiTraceHelper ascii;
    p2p.EnableAsciiAll(ascii.CreateFileStream("loose-routing-ipv6.tr"));

    uint32_t packetSize = 1024;
    uint32_t maxPacketCount = 5;
    Time interPacketInterval = Seconds(1.0);

    Ping6Helper ping6;
    ping6.SetLocal(hosts.Get(0)->GetObject<Ipv6>()->GetAddress(1, 0).GetAddress());
    ping6.SetRemote(targetAddr);
    ping6.SetAttribute("MaxPackets", UintegerValue(maxPacketCount));
    ping6.SetAttribute("Interval", TimeValue(interPacketInterval));
    ping6.SetAttribute("PacketSize", UintegerValue(packetSize));

    ApplicationContainer apps = ping6.Install(hosts.Get(0));
    apps.Start(Seconds(1.0));
    apps.Stop(Seconds(10.0));

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}