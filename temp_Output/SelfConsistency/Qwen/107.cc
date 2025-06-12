#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv6-static-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("FragmentationIpv6TwoMtu");

int main(int argc, char *argv[]) {
    // Enable logging
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create nodes
    Ptr<Node> src = CreateObject<Node>();
    Ptr<Node> n0 = CreateObject<Node>();
    Ptr<Node> r = CreateObject<Node>();
    Ptr<Node> n1 = CreateObject<Node>();
    Ptr<Node> dst = CreateObject<Node>();

    NodeContainer srcNode(src);
    NodeContainer n0Node(n0);
    NodeContainer rNode(r);
    NodeContainer n1Node(n1);
    NodeContainer dstNode(dst);

    // Create link between Src and n0
    NodeContainer srcN0(src, n0);
    CsmaHelper csmaSrcN0;
    csmaSrcN0.SetChannelAttribute("DataRate", DataRateValue(DataRate("100Mbps")));
    csmaSrcN0.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));
    NetDeviceContainer srcN0Devices = csmaSrcN0.Install(srcN0);

    // Create CSMA link between n0 and r with MTU 5000
    NodeContainer n0r(n0, r);
    CsmaHelper csmaN0R;
    csmaN0R.SetChannelAttribute("DataRate", DataRateValue(DataRate("100Mbps")));
    csmaN0R.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));
    NetDeviceContainer n0rDevices = csmaN0R.Install(n0r);
    DynamicCast<CsmaNetDevice>(n0rDevices.Get(1))->SetMtu(5000); // Set MTU on r's device

    // Create CSMA link between r and n1 with MTU 1500
    NodeContainer rn1(r, n1);
    CsmaHelper csmaRN1;
    csmaRN1.SetChannelAttribute("DataRate", DataRateValue(DataRate("100Mbps")));
    csmaRN1.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));
    NetDeviceContainer rn1Devices = csmaRN1.Install(rn1);
    DynamicCast<CsmaNetDevice>(rn1Devices.Get(0))->SetMtu(1500); // Set MTU on r's device

    // Create link between n1 and Dst
    NodeContainer n1Dst(n1, dst);
    CsmaHelper csmaN1Dst;
    csmaN1Dst.SetChannelAttribute("DataRate", DataRateValue(DataRate("100Mbps")));
    csmaN1Dst.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));
    NetDeviceContainer n1DstDevices = csmaN1Dst.Install(n1Dst);

    // Install Internet stack
    InternetStackHelper internet;
    internet.InstallAll();

    // Assign IPv6 addresses
    Ipv6AddressHelper ipv6;

    ipv6.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer srcN0Interfaces = ipv6.Assign(srcN0Devices);
    srcN0Interfaces.SetForwarding(1, true);
    srcN0Interfaces.SetDefaultRouteInAllNodes(1);

    ipv6.SetBase(Ipv6Address("2001:2::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer n0rInterfaces = ipv6.Assign(n0rDevices);
    n0rInterfaces.SetForwarding(1, true);
    n0rInterfaces.SetDefaultRouteInAllNodes(1);

    ipv6.SetBase(Ipv6Address("2001:3::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer rn1Interfaces = ipv6.Assign(rn1Devices);
    rn1Interfaces.SetForwarding(1, true);
    rn1Interfaces.SetDefaultRouteInAllNodes(1);

    ipv6.SetBase(Ipv6Address("2001:4::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer n1DstInterfaces = ipv6.Assign(n1DstDevices);
    n1DstInterfaces.SetForwarding(1, true);
    n1DstInterfaces.SetDefaultRouteInAllNodes(1);

    // Set up static routing
    Ipv6StaticRoutingHelper routingHelper;

    // From Src to Dst via n0 -> r -> n1
    Ptr<Ipv6StaticRouting> srcRouting = routingHelper.GetStaticRouting(src->GetObject<Ipv6>());
    srcRouting->AddHostRouteTo(Ipv6Address("2001:4::2"), Ipv6Address("2001:1::2"), 1);

    // On router r, set routes for both directions
    Ptr<Ipv6StaticRouting> rRouting = routingHelper.GetStaticRouting(r->GetObject<Ipv6>());
    rRouting->AddNetworkRouteTo(Ipv6Address("2001:1::"), Ipv6Prefix(64), 1); // To n0
    rRouting->AddNetworkRouteTo(Ipv6Address("2001:4::"), Ipv6Prefix(64), 2); // To n1

    // From Dst to Src via n1 -> r -> n0
    Ptr<Ipv6StaticRouting> dstRouting = routingHelper.GetStaticRouting(dst->GetObject<Ipv6>());
    dstRouting->AddHostRouteTo(Ipv6Address("2001:1::1"), Ipv6Address("2001:4::1"), 1);

    // Enable checksum computations (needed for fragmentation verification)
    Config::SetDefault("ns3::Ipv6L3Protocol::IpCheck", BooleanValue(true));

    // Setup applications
    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(dst);
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient(Ipv6Address("2001:4::2"), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(2000)); // Large enough to require fragmentation

    ApplicationContainer clientApps = echoClient.Install(src);
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Enable tracing
    AsciiTraceHelper asciiTraceHelper;
    Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream("fragmentation-ipv6-two-mtu.tr");
    csmaSrcN0.EnableAsciiAll(stream);
    csmaN0R.EnableAsciiAll(stream);
    csmaRN1.EnableAsciiAll(stream);
    csmaN1Dst.EnableAsciiAll(stream);

    csmaSrcN0.EnablePcapAll("fragmentation-ipv6-two-mtu");
    csmaN0R.EnablePcapAll("fragmentation-ipv6-two-mtu");
    csmaRN1.EnablePcapAll("fragmentation-ipv6-two-mtu");
    csmaN1Dst.EnablePcapAll("fragmentation-ipv6-two-mtu");

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}