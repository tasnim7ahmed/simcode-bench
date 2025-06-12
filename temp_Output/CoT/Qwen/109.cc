#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/csma-module.h"
#include "ns3/ipv6-routing-table-entry.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("LooseRoutingIPv6");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

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

    InternetStackHelper internet;
    internet.SetIpv4StackEnabled(false);
    internet.SetIpv6StackEnabled(true);
    internet.InstallAll();

    Ipv6AddressHelper ipv6;
    ipv6.SetBase(Ipv6Address("2001:db8::"), Ipv6Prefix(64));

    Ipv6InterfaceContainer host0_iface = ipv6.AssignWithoutAddress(host0_router0.Get(0));
    Ipv6InterfaceContainer router0_iface0 = ipv6.AssignWithoutAddress(host0_router0.Get(1));
    Ipv6InterfaceContainer router0_iface1 = ipv6.AssignWithoutAddress(router0_router1.Get(0));
    Ipv6InterfaceContainer router1_iface0 = ipv6.AssignWithoutAddress(router0_router1.Get(1));
    Ipv6InterfaceContainer router1_iface1 = ipv6.AssignWithoutAddress(router1_router2.Get(0));
    Ipv6InterfaceContainer router2_iface0 = ipv6.AssignWithoutAddress(router1_router2.Get(1));
    Ipv6InterfaceContainer router2_iface1 = ipv6.AssignWithoutAddress(router2_router3.Get(0));
    Ipv6InterfaceContainer router3_iface0 = ipv6.AssignWithoutAddress(router2_router3.Get(1));
    Ipv6InterfaceContainer router3_iface1 = ipv6.AssignWithoutAddress(router3_host1.Get(0));
    Ipv6InterfaceContainer host1_iface = ipv6.AssignWithoutAddress(router3_host1.Get(1));

    Ipv6Address host1Addr = host1_iface.GetAddress(0, 1);

    Ipv6StaticRoutingHelper routingHelper;

    Ptr<Ipv6StaticRouting> host0_routing = routingHelper.GetStaticRouting(hosts.Get(0)->GetObject<Ipv6>());
    host0_routing->AddHostRouteTo(host1Addr, router0_iface0.GetAddress(0, 1), 1);

    for (uint32_t i = 0; i < routers.GetN(); ++i) {
        Ptr<Ipv6StaticRouting> routerRouting = routingHelper.GetStaticRouting(routers.Get(i)->GetObject<Ipv6>());
        if (i == 0) {
            routerRouting->AddHostRouteTo(host1Addr, router0_iface1.GetAddress(0, 1), 2);
        } else if (i == 1) {
            routerRouting->AddHostRouteTo(host1Addr, router1_iface1.GetAddress(0, 1), 2);
        } else if (i == 2) {
            routerRouting->AddHostRouteTo(host1Addr, router2_iface1.GetAddress(0, 1), 2);
        } else if (i == 3) {
            routerRouting->AddHostRouteTo(host1Addr, router3_iface1.GetAddress(0, 1), 2);
        }
    }

    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(hosts.Get(1));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient(host1Addr, 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = echoClient.Install(hosts.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    AsciiTraceHelper ascii;
    p2p.EnableAsciiAll(ascii.CreateFileStream("loose-routing-ipv6.tr"));

    Simulator::Run();
    Simulator::Destroy();
    return 0;
}