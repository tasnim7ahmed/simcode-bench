#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/ipv6-routing-table-entry.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("Icmpv6RedirectExample");

void
SendRedirect(Ptr<Node> r1, Ipv6Address sta1Addr, Ipv6Address sta2Addr, Ipv6Address r2Addr)
{
    Ptr<Icmpv6L4Protocol> icmpv6 = r1->GetObject<Ipv6L3Protocol>()->GetIcmpv6();
    Icmpv6Redirect icmpRedirect;
    icmpRedirect.m_target = r2Addr;
    icmpRedirect.m_dst = sta2Addr;

    Ptr<Packet> p = Create<Packet>();
    icmpv6->SendRedirect(sta1Addr, r2Addr, sta2Addr, p);
}

int main(int argc, char *argv[])
{
    LogComponentEnable("Icmpv6L4Protocol", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create nodes: STA1, STA2, R1, R2
    NodeContainer staNodes, routerNodes;
    staNodes.Create(2);     // 0: STA1, 1: STA2
    routerNodes.Create(2);  // 0: R1, 1: R2

    // Link: STA1 <-> R1
    NodeContainer sta1r1(staNodes.Get(0), routerNodes.Get(0));
    // Link: R1 <-> R2
    NodeContainer r1r2(routerNodes.Get(0), routerNodes.Get(1));
    // Link: R2 <-> STA2
    NodeContainer r2sta2(routerNodes.Get(1), staNodes.Get(1));

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("1ms"));

    NetDeviceContainer d_sta1r1 = p2p.Install(sta1r1);
    NetDeviceContainer d_r1r2   = p2p.Install(r1r2);
    NetDeviceContainer d_r2sta2 = p2p.Install(r2sta2);

    // Install IPv6 stack
    InternetStackHelper stack;
    stack.Install(staNodes);
    stack.Install(routerNodes);

    // Assign IPv6 addresses
    Ipv6AddressHelper ipv6;
    // Subnet1: STA1 <-> R1
    ipv6.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer i_sta1r1 = ipv6.Assign(d_sta1r1);
    i_sta1r1.SetForwarding(0, false);    // STA1: no forward
    i_sta1r1.SetForwarding(1, true);     // R1: forward
    i_sta1r1.SetDefaultRouteInAllNodes(1);

    // Subnet2: R1 <-> R2
    ipv6.SetBase(Ipv6Address("2001:2::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer i_r1r2 = ipv6.Assign(d_r1r2);
    i_r1r2.SetForwarding(0, true);       // R1
    i_r1r2.SetForwarding(1, true);       // R2

    // Subnet3: R2 <-> STA2
    ipv6.SetBase(Ipv6Address("2001:3::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer i_r2sta2 = ipv6.Assign(d_r2sta2);
    i_r2sta2.SetForwarding(0, true);     // R2
    i_r2sta2.SetForwarding(1, false);    // STA2

    // Assign actual addresses for easier reference
    Ipv6Address sta1Addr = i_sta1r1.GetAddress(0, 1); // STA1
    Ipv6Address sta2Addr = i_r2sta2.GetAddress(1, 1); // STA2
    Ipv6Address r1If1    = i_sta1r1.GetAddress(1, 1); // R1 on link to STA1
    Ipv6Address r1If2    = i_r1r2.GetAddress(0, 1);   // R1 on link to R2
    Ipv6Address r2If1    = i_r1r2.GetAddress(1, 1);   // R2 on link to R1
    Ipv6Address r2If2    = i_r2sta2.GetAddress(0, 1); // R2 on link to STA2

    // Routing
    Ipv6StaticRoutingHelper ipv6RH;

    // STA1: Default route ::/0 -> R1's link-local (next-hop: r1If1)
    Ptr<Ipv6StaticRouting> sta1Static = ipv6RH.GetStaticRouting(staNodes.Get(0)->GetObject<Ipv6>());
    sta1Static->SetDefaultRoute(r1If1, 1);

    // R1: route to STA2 (2001:3::/64) via R2 (next-hop: r2If1)
    Ptr<Ipv6StaticRouting> r1Static = ipv6RH.GetStaticRouting(routerNodes.Get(0)->GetObject<Ipv6>());
    r1Static->AddNetworkRouteTo(Ipv6Address("2001:3::"), Ipv6Prefix(64), r2If1, 2);

    // R2: Default route ::/0 -> nothing; but route to 2001:1::/64 via R1 is not needed
    // STA2: Default route ::/0 -> R2's address (r2If2)
    Ptr<Ipv6StaticRouting> sta2Static = ipv6RH.GetStaticRouting(staNodes.Get(1)->GetObject<Ipv6>());
    sta2Static->SetDefaultRoute(r2If2, 1);

    // Applications: Put UdpEchoServer on STA2, UdpEchoClient on STA1
    uint16_t echoPort = 9;
    UdpEchoServerHelper echoServer(echoPort);
    ApplicationContainer serverApps = echoServer.Install(staNodes.Get(1));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(20.0));

    UdpEchoClientHelper echoClient(sta2Addr, echoPort);
    echoClient.SetAttribute("MaxPackets", UintegerValue(3));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(2.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(64));
    ApplicationContainer clientApps = echoClient.Install(staNodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(20.0));

    // Tracing
    AsciiTraceHelper ascii;
    Ptr<OutputStreamWrapper> stream = ascii.CreateFileStream("icmpv6-redirect.tr");
    p2p.EnableAsciiAll(stream);

    // After first packet, have R1 send ICMPv6 Redirect to STA1, for future echo requests to 2001:3::/64 via R2 (use R2's r2If1 address as the better next-hop)
    Simulator::Schedule(Seconds(5.0), &SendRedirect, routerNodes.Get(0), sta1Addr, sta2Addr, r2If1);

    Simulator::Stop(Seconds(20.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}