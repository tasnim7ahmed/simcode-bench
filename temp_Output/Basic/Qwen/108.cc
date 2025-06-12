#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv6-static-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("Icmpv6RedirectExample");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    NodeContainer sta1;
    sta1.Create(1);
    NodeContainer r1;
    r1.Create(1);
    NodeContainer r2;
    r2.Create(1);
    NodeContainer sta2;
    sta2.Create(1);

    NodeContainer sta1r1 = NodeContainer(sta1, r1);
    NodeContainer r1r2 = NodeContainer(r1, r2);
    NodeContainer r2sta2 = NodeContainer(r2, sta2);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", DataRateValue(DataRate("5Mbps")));
    p2p.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));

    NetDeviceContainer sta1r1Devices = p2p.Install(sta1r1);
    NetDeviceContainer r1r2Devices = p2p.Install(r1r2);
    NetDeviceContainer r2sta2Devices = p2p.Install(r2sta2);

    InternetStackHelper internet;
    internet.InstallAll();

    Ipv6AddressHelper ipv6;

    ipv6.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer sta1r1Interfaces = ipv6.Assign(sta1r1Devices);
    sta1r1Interfaces.SetForwarding(1, true); // R1 forwards

    ipv6.SetBase(Ipv6Address("2001:2::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer r1r2Interfaces = ipv6.Assign(r1r2Devices);
    r1r2Interfaces.SetForwarding(0, true); // R1 forwards
    r1r2Interfaces.SetForwarding(1, true); // R2 forwards

    ipv6.SetBase(Ipv6Address("2001:3::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer r2sta2Interfaces = ipv6.Assign(r2sta2Devices);
    r2sta2Interfaces.SetForwarding(1, true); // STA2 does not forward

    // Assign addresses to nodes
    Ipv6InterfaceContainer sta1Interface = Ipv6InterfaceContainer();
    sta1Interface.Add(sta1r1Interfaces.Get(0));
    Ipv6InterfaceContainer r1Interface1 = Ipv6InterfaceContainer();
    r1Interface1.Add(sta1r1Interfaces.Get(1));
    Ipv6InterfaceContainer r1Interface2 = Ipv6InterfaceContainer();
    r1Interface2.Add(r1r2Interfaces.Get(0));
    Ipv6InterfaceContainer r2Interface1 = Ipv6InterfaceContainer();
    r2Interface1.Add(r1r2Interfaces.Get(1));
    Ipv6InterfaceContainer r2Interface2 = Ipv6InterfaceContainer();
    r2Interface2.Add(r2sta2Interfaces.Get(0));
    Ipv6InterfaceContainer sta2Interface = Ipv6InterfaceContainer();
    sta2Interface.Add(r2sta2Interfaces.Get(1));

    // Set default routes
    Ipv6StaticRoutingHelper routingHelper;

    // STA1 default route via R1
    Ptr<Ipv6StaticRouting> sta1Routing = routingHelper.GetStaticRouting(sta1.Get(0)->GetObject<Ipv6>());
    sta1Routing->AddDefaultRoute(r1Interface1.GetAddress(0), 1);

    // STA2 default route via R2
    Ptr<Ipv6StaticRouting> sta2Routing = routingHelper.GetStaticRouting(sta2.Get(0)->GetObject<Ipv6>());
    sta2Routing->AddDefaultRoute(r2Interface2.GetAddress(0), 1);

    // R1 has static route to STA2 (behind R2)
    Ptr<Ipv6StaticRouting> r1Routing = routingHelper.GetStaticRouting(r1.Get(0)->GetObject<Ipv6>());
    r1Routing->AddHostRouteTo(r2sta2Interfaces.GetAddress(1, 1), r1Interface2.GetAddress(0), 2);

    // Create echo server on STA2
    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(sta2.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // Create first echo request from STA1 to STA2 at 2.0s
    UdpEchoClientHelper echoClient(r2sta2Interfaces.GetAddress(1, 1), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(1));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(512));
    ApplicationContainer clientApps = echoClient.Install(sta1.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Schedule redirection
    Simulator::Schedule(Seconds(3.0), []() {
        Ptr<Node> r1Node = NodeList::GetNode(1);
        Ptr<Ipv6L3Protocol> r1Ipv6 = r1Node->GetObject<Ipv6L3Protocol>();
        if (r1Ipv6) {
            r1Ipv6->SendIcmpv6Redirection(
                Ipv6Address("2001:1::1"), // origin
                Ipv6Address("2001:3::2"), // target
                Ipv6Address("2001:2::2")  // destination
            );
        }
    });

    // Second echo after redirection at 4.0s
    Simulator::Schedule(Seconds(4.0), [&]() {
        UdpEchoClientHelper echoClient2(r2sta2Interfaces.GetAddress(1, 1), 9);
        echoClient2.SetAttribute("MaxPackets", UintegerValue(1));
        echoClient2.SetAttribute("Interval", TimeValue(Seconds(1.0)));
        echoClient2.SetAttribute("PacketSize", UintegerValue(512));
        ApplicationContainer clientApps2 = echoClient2.Install(sta1.Get(0));
        clientApps2.Start(Seconds(0.0));
        clientApps2.Stop(Seconds(10.0));
        Simulator::Schedule(Seconds(10.0), &Simulator::Stop);
    });

    AsciiTraceHelper ascii;
    p2p.EnableAsciiAll(ascii.CreateFileStream("icmpv6-redirect.tr"));
    p2p.EnablePcapAll("icmpv6-redirect");

    Simulator::Run();
    Simulator::Destroy();
    return 0;
}