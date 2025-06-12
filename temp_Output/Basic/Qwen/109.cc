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
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

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
    Ipv6InterfaceContainer host0_router0_interfaces = ipv6.Assign(host0_router0);

    ipv6.SetBase(Ipv6Address("2001:2::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer router0_router1_interfaces = ipv6.Assign(router0_router1);

    ipv6.SetBase(Ipv6Address("2001:3::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer router1_router2_interfaces = ipv6.Assign(router1_router2);

    ipv6.SetBase(Ipv6Address("2001:4::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer router2_router3_interfaces = ipv6.Assign(router2_router3);

    ipv6.SetBase(Ipv6Address("2001:5::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer router3_host1_interfaces = ipv6.Assign(router3_host1);

    Ipv6StaticRoutingHelper routingHelper;

    for (uint32_t i = 0; i < hosts.Get(0)->GetObject<Ipv6>()->GetNInterfaces(); ++i) {
        Ptr<Ipv6StaticRouting> host0_routing = routingHelper.GetStaticRouting(hosts.Get(0)->GetObject<Ipv6>());
        host0_routing->AddHostRouteTo(hosts.Get(1)->GetObject<Ipv6>()->GetAddress(1, 0).GetAddress(), 5, 1);
    }

    for (uint32_t i = 0; i < routers.Get(0)->GetObject<Ipv6>()->GetNInterfaces(); ++i) {
        Ptr<Ipv6StaticRouting> router0_routing = routingHelper.GetStaticRouting(routers.Get(0)->GetObject<Ipv6>());
        router0_routing->AddNetworkRouteTo(Ipv6Address("2001:5::"), Ipv6Prefix(64), 2, 1);
    }

    for (uint32_t i = 0; i < routers.Get(1)->GetObject<Ipv6>()->GetNInterfaces(); ++i) {
        Ptr<Ipv6StaticRouting> router1_routing = routingHelper.GetStaticRouting(routers.Get(1)->GetObject<Ipv6>());
        router1_routing->AddNetworkRouteTo(Ipv6Address("2001:5::"), Ipv6Prefix(64), 3, 1);
    }

    for (uint32_t i = 0; i < routers.Get(2)->GetObject<Ipv6>()->GetNInterfaces(); ++i) {
        Ptr<Ipv6StaticRouting> router2_routing = routingHelper.GetStaticRouting(routers.Get(2)->GetObject<Ipv6>());
        router2_routing->AddNetworkRouteTo(Ipv6Address("2001:5::"), Ipv6Prefix(64), 4, 1);
    }

    for (uint32_t i = 0; i < routers.Get(3)->GetObject<Ipv6>()->GetNInterfaces(); ++i) {
        Ptr<Ipv6StaticRouting> router3_routing = routingHelper.GetStaticRouting(routers.Get(3)->GetObject<Ipv6>());
        router3_routing->AddHostRouteTo(hosts.Get(1)->GetObject<Ipv6>()->GetAddress(1, 0).GetAddress(), 2, 1);
    }

    uint32_t packetSize = 1024;
    uint32_t maxPacketCount = 5;
    Time interPacketInterval = Seconds(1.0);

    PingHelper ping(hosts.Get(1)->GetObject<Ipv6>()->GetAddress(1, 0).GetAddress());
    ping.SetAttribute("MaxPackets", UintegerValue(maxPacketCount));
    ping.SetAttribute("Interval", TimeValue(interPacketInterval));
    ping.SetAttribute("PacketSize", UintegerValue(packetSize));

    ApplicationContainer pingApp = ping.Install(hosts.Get(0));
    pingApp.Start(Seconds(2.0));
    pingApp.Stop(Seconds(10.0));

    AsciiTraceHelper ascii;
    p2p.EnableAsciiAll(ascii.CreateFileStream("loose-routing-ipv6.tr"));

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}