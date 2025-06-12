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

    // Enable logging for routing and ICMPv6
    LogComponentEnable("Ipv6L3Protocol", LOG_LEVEL_ALL);
    LogComponentEnable("Icmpv6L4Protocol", LOG_LEVEL_INFO);
    LogComponentEnable("Ipv6StaticRouting", LOG_LEVEL_ALL);

    // Create nodes
    NodeContainer hosts;
    hosts.Create(2);  // Host 0 and Host 1

    NodeContainer routers;
    routers.Create(4);  // Router 0 to Router 3

    // Create point-to-point links
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", DataRateValue(DataRate("5Mbps")));
    p2p.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));

    // Link topology:
    // Host0 -- R0 -- R1 -- R2 -- R3 -- Host1

    NetDeviceContainer host0R0 = p2p.Install(hosts.Get(0), routers.Get(0));
    NetDeviceContainer r0r1 = p2p.Install(routers.Get(0), routers.Get(1));
    NetDeviceContainer r1r2 = p2p.Install(routers.Get(1), routers.Get(2));
    NetDeviceContainer r2r3 = p2p.Install(routers.Get(2), routers.Get(3));
    NetDeviceContainer r3Host1 = p2p.Install(routers.Get(3), hosts.Get(1));

    InternetStackHelper internet;
    internet.InstallAll();

    Ipv6AddressHelper ipv6;

    // Assign IPv6 addresses
    ipv6.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer host0R0Ifaces = ipv6.Assign(host0R0);
    host0R0Ifaces.SetForwarding(0, true);
    host0R0Ifaces.SetDefaultRouteInfiniteMetric(0, true);
    host0R0Ifaces.SetForwarding(1, true);

    ipv6.SetBase(Ipv6Address("2001:2::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer r0r1Ifaces = ipv6.Assign(r0r1);
    r0r1Ifaces.SetForwarding(0, true);
    r0r1Ifaces.SetForwarding(1, true);

    ipv6.SetBase(Ipv6Address("2001:3::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer r1r2Ifaces = ipv6.Assign(r1r2);
    r1r2Ifaces.SetForwarding(0, true);
    r1r2Ifaces.SetForwarding(1, true);

    ipv6.SetBase(Ipv6Address("2001:4::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer r2r3Ifaces = ipv6.Assign(r2r3);
    r2r3Ifaces.SetForwarding(0, true);
    r2r3Ifaces.SetForwarding(1, true);

    ipv6.SetBase(Ipv6Address("2001:5::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer r3Host1Ifaces = ipv6.Assign(r3Host1);
    r3Host1Ifaces.SetForwarding(0, true);
    r3Host1Ifaces.SetForwarding(1, true);
    r3Host1Ifaces.SetDefaultRouteInfiniteMetric(1, true);

    // Set up loose source routing from Host0 to Host1 via R0 -> R1 -> R2 -> R3
    Ipv6StaticRoutingHelper routingHelper;

    Ptr<Ipv6> host0Ipv6 = hosts.Get(0)->GetObject<Ipv6>();
    Ptr<Ipv6StaticRouting> host0Routing = routingHelper.GetStaticRouting(host0Ipv6);

    Ipv6Address host1Addr = r3Host1Ifaces.GetAddress(1, 1);  // Host1's interface facing Router3

    // Add route with loose hops: Host0 -> R0 -> R1 -> R2 -> R3 -> Host1
    host0Routing->AddHostRouteTo(host1Addr, Ipv6Address("2001:1::2"), 1);  // Next hop: R0
    host0Routing->SetDefaultRoute(Ipv6Address("2001:1::2"), 1);  // Default to R0

    // Install applications
    uint16_t echoPort = 9;

    // On Host1: Echo server
    Ipv6EchoServerHelper echoServer(echoPort);
    ApplicationContainer serverApps = echoServer.Install(hosts.Get(1));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // On Host0: Echo client sending to Host1 using loose source routing
    Ipv6EchoClientHelper echoClient(host1Addr, echoPort);
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));
    echoClient.SetAttribute("Count", UintegerValue(5));

    ApplicationContainer clientApps = echoClient.Install(hosts.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Tracing
    AsciiTraceHelper ascii;
    p2p.EnableAsciiAll(ascii.CreateFileStream("loose-routing-ipv6.tr"));
    p2p.EnablePcapAll("loose-routing-ipv6");

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}