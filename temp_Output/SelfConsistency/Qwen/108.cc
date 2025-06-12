#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv6-static-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("Icmpv6RedirectExample");

int main(int argc, char *argv[]) {
    // Enable logging for Icmpv6 and routing components
    LogComponentEnable("Icmpv6L4Protocol", LOG_LEVEL_ALL);
    LogComponentEnable("Ipv6StaticRouting", LOG_LEVEL_ALL);
    LogComponentEnable("Node", LOG_LEVEL_INFO);

    // Create nodes
    Ptr<Node> sta1 = CreateObject<Node>();
    Ptr<Node> r1 = CreateObject<Node>();
    Ptr<Node> r2 = CreateObject<Node>();
    Ptr<Node> sta2 = CreateObject<Node>();

    // Create the point-to-point links
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // Install internet stacks
    InternetStackHelper internetv6;
    internetv6.SetIpv4StackEnabled(false);
    internetv6.SetIpv6StackEnabled(true);
    internetv6.InstallAll();

    // Create devices and link nodes
    NetDeviceContainer sta1_r1 = p2p.Install(sta1, r1);
    NetDeviceContainer r1_r2 = p2p.Install(r1, r2);
    NetDeviceContainer r2_sta2 = p2p.Install(r2, sta2);

    // Assign IPv6 addresses
    Ipv6AddressHelper ipv6;

    ipv6.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer sta1_r1_ifaces = ipv6.Assign(sta1_r1);

    ipv6.SetBase(Ipv6Address("2001:2::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer r1_r2_ifaces = ipv6.Assign(r1_r2);

    ipv6.SetBase(Ipv6Address("2001:3::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer r2_sta2_ifaces = ipv6.Assign(r2_sta2);

    // Assign addresses to interfaces
    sta1_r1_ifaces.SetForwarding(0, true);
    sta1_r1_ifaces.SetDefaultRouteInfiniteMetric(0, true);
    sta1_r1_ifaces.SetAddress(1, Ipv6Address("2001:1::1"), 64); // R1's interface

    r1_r2_ifaces.SetForwarding(0, true);
    r1_r2_ifaces.SetAddress(1, Ipv6Address("2001:2::1"), 64); // R2's interface

    r2_sta2_ifaces.SetForwarding(0, true);
    r2_sta2_ifaces.SetAddress(1, Ipv6Address("2001:3::1"), 64); // STA2's interface

    // Set up default routes
    Ipv6StaticRoutingHelper routingHelper;

    // STA1's default route via R1
    Ptr<Ipv6StaticRouting> sta1_routing = routingHelper.GetStaticRouting(sta1->GetObject<Ipv6>());
    sta1_routing->AddNetworkRouteTo(Ipv6Address::GetZero(), Ipv6Prefix::GetZero(), Ipv6Address("2001:1::1"), 1);

    // R1's static route to STA2 (via R2)
    Ptr<Ipv6StaticRouting> r1_routing = routingHelper.GetStaticRouting(r1->GetObject<Ipv6>());
    r1_routing->AddHostRouteTo(Ipv6Address("2001:3::2"), Ipv6Address("2001:2::2"), 2);

    // R2's default route to STA2
    Ptr<Ipv6StaticRouting> r2_routing = routingHelper.GetStaticRouting(r2->GetObject<Ipv6>());
    r2_routing->AddNetworkRouteTo(Ipv6Address::GetZero(), Ipv6Prefix::GetZero(), Ipv6Address("2001:3::1"), 1);

    // STA2's default route via R2
    Ptr<Ipv6StaticRouting> sta2_routing = routingHelper.GetStaticRouting(sta2->GetObject<Ipv6>());
    sta2_routing->AddNetworkRouteTo(Ipv6Address::GetZero(), Ipv6Prefix::GetZero(), Ipv6Address("2001:3::2"), 1);

    // Setup Echo application from STA1 to STA2
    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(sta2);
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient(Ipv6Address("2001:3::2"), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(2));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = echoClient.Install(sta1);
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Enable packet tracing
    AsciiTraceHelper asciiTraceHelper;
    Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream("icmpv6-redirect.tr");
    p2p.EnableAsciiAll(stream);

    // Schedule simulation
    Simulator::Stop(Seconds(11.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}