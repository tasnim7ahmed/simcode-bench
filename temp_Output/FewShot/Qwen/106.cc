#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable logging for fragmentation and routing
    LogComponentEnable("Ipv6L3Protocol", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create nodes
    NodeContainer srcNode;
    srcNode.Create(1);

    NodeContainer n0;
    n0.Create(1);

    NodeContainer r1;
    r1.Create(1);

    NodeContainer n1;
    n1.Create(1);

    NodeContainer r2;
    r2.Create(1);

    NodeContainer n2;
    n2.Create(1);

    // Build links with varying MTUs
    PointToPointHelper p2pLink1;
    p2pLink1.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2pLink1.SetChannelAttribute("Delay", StringValue("2ms"));
    p2pLink1.SetDeviceAttribute("Mtu", UintegerValue(5000));

    PointToPointHelper p2pLink2;
    p2pLink2.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2pLink2.SetChannelAttribute("Delay", StringValue("2ms"));
    p2pLink2.SetDeviceAttribute("Mtu", UintegerValue(2000));

    CsmaHelper csmaLink1;
    csmaLink1.SetChannelAttribute("DataRate", StringValue("10Mbps"));
    csmaLink1.SetChannelAttribute("Delay", StringValue("2ms"));
    csmaLink1.SetDeviceAttribute("Mtu", UintegerValue(1500));

    // Install internet stack with IPv6
    InternetStackHelper internetv6;
    internetv6.SetIpv4StackEnabled(false);
    internetv6.SetIpv6StackEnabled(true);
    internetv6.InstallAll();

    // Link nodes: Src <-> n0 <-> r1 <-> n1 <-> r2 <-> n2

    NetDeviceContainer devSrc_n0 = p2pLink1.Install(srcNode.Get(0), n0.Get(0));
    NetDeviceContainer devn0_r1 = p2pLink2.Install(n0.Get(0), r1.Get(0));
    NetDeviceContainer devr1_n1 = csmaLink1.Install(n1, r1.Get(0));
    NetDeviceContainer devn1_r2 = p2pLink2.Install(n1.Get(0), r2.Get(0));
    NetDeviceContainer devr2_n2 = p2pLink1.Install(r2.Get(0), n2.Get(0));

    // Assign IPv6 addresses
    Ipv6AddressHelper ipv6;

    ipv6.SetBase(Ipv6Address("2001:db8::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer iSrc_n0 = ipv6.Assign(devSrc_n0);
    iSrc_n0.SetForwarding(0, true);
    iSrc_n0.SetDefaultRouteInfiniteMetric(0, false);
    iSrc_n0.SetForwarding(1, true);

    ipv6.NewNetwork();
    Ipv6InterfaceContainer in0_r1 = ipv6.Assign(devn0_r1);
    in0_r1.SetForwarding(0, true);
    in0_r1.SetForwarding(1, true);

    ipv6.NewNetwork();
    Ipv6InterfaceContainer ir1_n1 = ipv6.Assign(devr1_n1);
    ir1_n1.SetForwarding(0, true);
    for (uint32_t i = 1; i < ir1_n1.GetN(); ++i) {
        ir1_n1.SetForwarding(i, true);
    }

    ipv6.NewNetwork();
    Ipv6InterfaceContainer in1_r2 = ipv6.Assign(devn1_r2);
    in1_r2.SetForwarding(0, true);
    in1_r2.SetForwarding(1, true);

    ipv6.NewNetwork();
    Ipv6InterfaceContainer ir2_n2 = ipv6.Assign(devr2_n2);
    ir2_n2.SetForwarding(0, true);
    ir2_n2.SetForwarding(1, false);
    ir2_n2.SetDefaultRouteInfiniteMetric(1, true);

    // Manually set up routing
    Ptr<Ipv6RoutingTableEntry> route1 = Ipv6RoutingTableEntry::CreateNetworkRouteTo(
        Ipv6Address("2001:db8:0:3::"), Ipv6Prefix(64), 2, Ipv6Address("2001:db8:0:1:2"));
    r1.Get(0)->GetObject<Ipv6>()->GetRoutingProtocol()->NotifyAddRoute(route1);

    Ptr<Ipv6RoutingTableEntry> route2 = Ipv6RoutingTableEntry::CreateNetworkRouteTo(
        Ipv6Address("2001:db8:0:4::"), Ipv6Prefix(64), 3, Ipv6Address("2001:db8:0:2:2"));
    r1.Get(0)->GetObject<Ipv6>()->GetRoutingProtocol()->NotifyAddRoute(route2);

    Ptr<Ipv6RoutingTableEntry> route3 = Ipv6RoutingTableEntry::CreateNetworkRouteTo(
        Ipv6Address("2001:db8::"), Ipv6Prefix(64), 1, Ipv6Address("2001:db8:0:0:1"));
    r2.Get(0)->GetObject<Ipv6>()->GetRoutingProtocol()->NotifyAddRoute(route3);

    // Set up UDP Echo Server on node n2
    uint16_t port = 9;
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApp = echoServer.Install(n2.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Set up UDP Echo Client on node Src
    Ipv6Address serverIp = ir2_n2.GetAddress(1, 1); // Address of n2
    UdpEchoClientHelper echoClient(serverIp, port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(2000)); // Big enough to fragment

    ApplicationContainer clientApp = echoClient.Install(srcNode.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Setup tracing
    AsciiTraceHelper asciiTraceHelper;
    p2pLink1.EnableAsciiAll(asciiTraceHelper.CreateFileStream("fragmentation-ipv6-PMTU.tr"));
    p2pLink2.EnableAsciiAll(asciiTraceHelper.CreateFileStream("fragmentation-ipv6-PMTU.tr"));
    csmaLink1.EnableAsciiAll(asciiTraceHelper.CreateFileStream("fragmentation-ipv6-PMTU.tr"));

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}