#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv6-routing-table-entry.h"
#include "ns3/ipv6-static-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("Icmpv6RedirectSimulation");

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

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer sta1_r1 = p2p.Install(sta1.Get(0), r1.Get(0));
    NetDeviceContainer r1_r2 = p2p.Install(r1.Get(0), r2.Get(0));
    NetDeviceContainer r2_sta2 = p2p.Install(r2.Get(0), sta2.Get(0));

    InternetStackHelper internet;
    internet.InstallAll();

    Ipv6AddressHelper ipv6;
    ipv6.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer sta1_r1_ifaces = ipv6.Assign(sta1_r1);
    Ipv6InterfaceContainer r1_r2_ifaces = ipv6.Assign(r1_r2);
    Ipv6InterfaceContainer r2_sta2_ifaces = ipv6.Assign(r2_sta2);

    // Assign addresses manually for better control
    // STA1 <-> R1
    sta1_r1_ifaces.SetForwarding(0, true);
    sta1_r1_ifaces.SetDefaultRouteInOneDirection(0);
    sta1_r1_ifaces.SetForwarding(1, true);
    r1_r2_ifaces.SetForwarding(0, true);
    r1_r2_ifaces.SetDefaultRouteInOneDirection(0);
    r1_r2_ifaces.SetForwarding(1, true);
    r2_sta2_ifaces.SetForwarding(0, true);
    r2_sta2_ifaces.SetDefaultRouteInOneDirection(0);
    r2_sta2_ifaces.SetForwarding(1, true);

    // Static routes on routers
    Ipv6StaticRoutingHelper routingHelper;
    Ptr<Ipv6StaticRouting> r1_routing = routingHelper.GetStaticRouting(r1.Get(0)->GetObject<Ipv6>());
    Ptr<Ipv6StaticRouting> r2_routing = routingHelper.GetStaticRouting(r2.Get(0)->GetObject<Ipv6>());

    Ipv6Address sta2Addr = r2_sta2_ifaces.GetAddress(1, 0);
    Ipv6Address r2AddrOnR1 = r1_r2_ifaces.GetAddress(0, 0);
    r1_routing->AddHostRouteTo(sta2Addr, r2AddrOnR1, 1); // route to STA2 via R2

    // Echo server on STA2
    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(sta2.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // Echo client on STA1
    UdpEchoClientHelper echoClient(sta2Addr, 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(1));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(512));

    ApplicationContainer clientApps = echoClient.Install(sta1.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    AsciiTraceHelper ascii;
    p2p.EnableAsciiAll(ascii.CreateFileStream("icmpv6-redirect.tr"));
    p2p.EnablePcapAll("icmpv6-redirect");

    Simulator::Run();
    Simulator::Destroy();
    return 0;
}