#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/csma-module.h"
#include "ns3/ipv6-static-routing-helper.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable logging for ICMPv6 redirection
    LogComponentEnable("Ipv6L3Protocol", LOG_LEVEL_INFO);
    LogComponentEnable("Icmpv6L4Protocol", LOG_LEVEL_INFO);

    // Create nodes
    Ptr<Node> sta1 = CreateObject<Node>();
    Ptr<Node> r1 = CreateObject<Node>();
    Ptr<Node> r2 = CreateObject<Node>();
    Ptr<Node> sta2 = CreateObject<Node>();

    NodeContainer net1(sta1, r1);
    NodeContainer net2(r1, r2);
    NodeContainer net3(r2, sta2);

    // Setup point-to-point links
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer dev1 = p2p.Install(net1);
    NetDeviceContainer dev2 = p2p.Install(net2);
    NetDeviceContainer dev3 = p2p.Install(net3);

    // Install internet stack
    InternetStackHelper stack;
    stack.Install(sta1);
    stack.Install(r1);
    stack.Install(r2);
    stack.Install(sta2);

    // Assign IP addresses
    Ipv6AddressHelper ipv6;

    ipv6.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer sta1_r1 = ipv6.Assign(dev1);
    sta1_r1.SetForwarding(0, true); // r1 is a router
    sta1_r1.SetDefaultRouteInCaseOfAddAddress(0, true);
    sta1_r1.SetDefaultRouteInCaseOfAddAddress(1, true);

    ipv6.SetBase(Ipv6Address("2001:2::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer r1_r2 = ipv6.Assign(dev2);
    r1_r2.SetForwarding(0, true); // r1 is a router
    r1_r2.SetForwarding(1, true); // r2 is a router

    ipv6.SetBase(Ipv6Address("2001:3::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer r2_sta2 = ipv6.Assign(dev3);
    r2_sta2.SetForwarding(1, true); // r2 is a router

    // Set up default routes
    Ipv6StaticRoutingHelper routingHelper;

    // STA1's default route via R1
    Ptr<Ipv6StaticRouting> sta1_routing = routingHelper.GetStaticRouting(sta1->GetObject<Ipv6>());
    sta1_routing->SetDefaultRoute("2001:1::2", 1); // via R1

    // R1 static route to STA2 via R2
    Ptr<Ipv6StaticRouting> r1_routing = routingHelper.GetStaticRouting(r1->GetObject<Ipv6>());
    r1_routing->AddNetworkRouteTo("2001:3::/64", "2001:2::2", 2);

    // R2's default route (to itself, not strictly needed)
    // STA2's default route via R2
    Ptr<Ipv6StaticRouting> sta2_routing = routingHelper.GetStaticRouting(sta2->GetObject<Ipv6>());
    sta2_routing->SetDefaultRoute("2001:3::1", 1); // via R2

    // Echo client application from STA1 to STA2
    uint16_t echoPort = 9;
    UdpEchoServerHelper echoServer(echoPort);
    ApplicationContainer serverApp = echoServer.Install(sta2);
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient(Ipv6Address("2001:3::2"), echoPort);
    echoClient.SetAttribute("MaxPackets", UintegerValue(2));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp = echoClient.Install(sta1);
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Enable ASCII and PCAP tracing
    AsciiTraceHelper ascii;
    p2p.EnableAsciiAll(ascii.CreateFileStream("icmpv6-redirect.tr"));
    p2p.EnablePcapAll("icmpv6-redirect");

    // Run simulation
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}