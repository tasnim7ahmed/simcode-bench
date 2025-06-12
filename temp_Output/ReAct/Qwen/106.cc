#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv6-routing-table-entry.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("FragmentationIpv6PMTU");

int main(int argc, char *argv[]) {
    LogComponentEnable("FragmentationIpv6PMTU", LOG_LEVEL_INFO);
    LogComponentEnable("Ipv6L3Protocol", LOG_LEVEL_ALL);
    LogComponentEnable("Icmpv6L4Protocol", LOG_LEVEL_INFO);

    NodeContainer nodes;
    nodes.Create(6); // Src, n0, r1, n1, r2, n2

    Ptr<Node> src = nodes.Get(0);
    Ptr<Node> n0 = nodes.Get(1);
    Ptr<Node> r1 = nodes.Get(2);
    Ptr<Node> n1 = nodes.Get(3);
    Ptr<Node> r2 = nodes.Get(4);
    Ptr<Node> n2 = nodes.Get(5);

    // Links and channels
    PointToPointHelper p2p_r1r2;
    p2p_r1r2.SetDeviceAttribute("Mtu", UintegerValue(2000));
    p2p_r1r2.SetChannelAttribute("Delay", StringValue("2ms"));

    PointToPointHelper p2p_srcn0;
    p2p_srcn0.SetDeviceAttribute("Mtu", UintegerValue(5000));
    p2p_srcn0.SetChannelAttribute("Delay", StringValue("2ms"));

    CsmaHelper csma_n0r1;
    csma_n0r1.SetChannelAttribute("DataRate", DataRateValue(DataRate("100Mbps")));
    csma_n0r1.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));
    csma_n0r1.SetDeviceAttribute("Mtu", UintegerValue(5000));

    CsmaHelper csma_n1r1;
    csma_n1r1.SetChannelAttribute("DataRate", DataRateValue(DataRate("100Mbps")));
    csma_n1r1.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));
    csma_n1r1.SetDeviceAttribute("Mtu", UintegerValue(2000));

    CsmaHelper csma_n2r2;
    csma_n2r2.SetChannelAttribute("DataRate", DataRateValue(DataRate("100Mbps")));
    csma_n2r2.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));
    csma_n2r2.SetDeviceAttribute("Mtu", UintegerValue(1500));

    // Install internet stacks
    InternetStackHelper stack;
    stack.Install(nodes);

    // Link segments
    NetDeviceContainer dev_srcn0 = p2p_srcn0.Install(src, n0);
    NetDeviceContainer dev_n0r1 = csma_n0r1.Install(n0, r1);
    NetDeviceContainer dev_r1r2 = p2p_r1r2.Install(r1, r2);
    NetDeviceContainer dev_n1r1 = csma_n1r1.Install(n1, r1);
    NetDeviceContainer dev_n2r2 = csma_n2r2.Install(n2, r2);

    // Assign IPv6 addresses
    Ipv6AddressHelper address;

    address.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer if_srcn0 = address.Assign(dev_srcn0);
    if_srcn0.SetForwarding(0, true);
    if_srcn0.SetDefaultRouteInfiniteMetric(0, true);
    if_srcn0.SetForwarding(1, true);
    if_srcn0.SetDefaultRouteInfiniteMetric(1, true);

    address.SetBase(Ipv6Address("2001:2::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer if_n0r1 = address.Assign(dev_n0r1);
    if_n0r1.SetForwarding(0, true);
    if_n0r1.SetDefaultRouteInfiniteMetric(0, true);
    if_n0r1.SetForwarding(1, true);
    if_n0r1.SetDefaultRouteInfiniteMetric(1, true);

    address.SetBase(Ipv6Address("2001:3::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer if_r1r2 = address.Assign(dev_r1r2);
    if_r1r2.SetForwarding(0, true);
    if_r1r2.SetDefaultRouteInfiniteMetric(0, true);
    if_r1r2.SetForwarding(1, true);
    if_r1r2.SetDefaultRouteInfiniteMetric(1, true);

    address.SetBase(Ipv6Address("2001:4::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer if_n1r1 = address.Assign(dev_n1r1);
    if_n1r1.SetForwarding(0, true);
    if_n1r1.SetDefaultRouteInfiniteMetric(0, true);
    if_n1r1.SetForwarding(1, true);
    if_n1r1.SetDefaultRouteInfiniteMetric(1, true);

    address.SetBase(Ipv6Address("2001:5::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer if_n2r2 = address.Assign(dev_n2r2);
    if_n2r2.SetForwarding(0, true);
    if_n2r2.SetDefaultRouteInfiniteMetric(0, true);
    if_n2r2.SetForwarding(1, true);
    if_n2r2.SetDefaultRouteInfiniteMetric(1, true);

    // Set default routes manually
    Ipv6StaticRoutingHelper routingHelper;

    // Src -> n0 -> r1 -> n1/r2 -> n2
    Ptr<Ipv6StaticRouting> srcRouting = routingHelper.GetStaticRouting(if_srcn0.Get(0));
    srcRouting->AddNetworkRouteTo(Ipv6Address("2001:2::"), Ipv6Prefix(64), 2);
    srcRouting->AddNetworkRouteTo(Ipv6Address("2001:3::"), Ipv6Prefix(64), 2);
    srcRouting->AddNetworkRouteTo(Ipv6Address("2001:4::"), Ipv6Prefix(64), 2);
    srcRouting->AddNetworkRouteTo(Ipv6Address("2001:5::"), Ipv6Prefix(64), 2);

    // n0
    Ptr<Ipv6StaticRouting> n0Routing = routingHelper.GetStaticRouting(if_n0r1.Get(0));
    n0Routing->AddNetworkRouteTo(Ipv6Address("2001:3::"), Ipv6Prefix(64), 2);
    n0Routing->AddNetworkRouteTo(Ipv6Address("2001:4::"), Ipv6Prefix(64), 2);
    n0Routing->AddNetworkRouteTo(Ipv6Address("2001:5::"), Ipv6Prefix(64), 2);

    // r1
    Ptr<Ipv6StaticRouting> r1Routing = routingHelper.GetStaticRouting(if_r1r2.Get(0));
    r1Routing->AddNetworkRouteTo(Ipv6Address("2001:4::"), Ipv6Prefix(64), 1);
    r1Routing->AddNetworkRouteTo(Ipv6Address("2001:5::"), Ipv6Prefix(64), 3);

    // r2
    Ptr<Ipv6StaticRouting> r2Routing = routingHelper.GetStaticRouting(if_r1r2.Get(1));
    r2Routing->AddNetworkRouteTo(Ipv6Address("2001:2::"), Ipv6Prefix(64), 3);
    r2Routing->AddNetworkRouteTo(Ipv6Address("2001:4::"), Ipv6Prefix(64), 3);

    // UDP Echo Server on n1
    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(n1);
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // UDP Echo Client from Src to n2
    UdpEchoClientHelper echoClient(if_n2r2.GetAddress(1, 1), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(4000)); // Large packet for fragmentation

    ApplicationContainer clientApp = echoClient.Install(src);
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Tracing
    AsciiTraceHelper ascii;
    p2p_r1r2.EnableAsciiAll(ascii.CreateFileStream("fragmentation-ipv6-PMTU.tr"));
    csma_n0r1.EnableAsciiAll(ascii.CreateFileStream("fragmentation-ipv6-PMTU.tr"));
    csma_n1r1.EnableAsciiAll(ascii.CreateFileStream("fragmentation-ipv6-PMTU.tr"));
    csma_n2r2.EnableAsciiAll(ascii.CreateFileStream("fragmentation-ipv6-PMTU.tr"));
    p2p_srcn0.EnableAsciiAll(ascii.CreateFileStream("fragmentation-ipv6-PMTU.tr"));

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}