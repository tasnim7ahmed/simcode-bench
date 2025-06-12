#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Create nodes
    Ptr<Node> src = CreateObject<Node>();
    Ptr<Node> n0 = CreateObject<Node>();
    Ptr<Node> r = CreateObject<Node>();
    Ptr<Node> n1 = CreateObject<Node>();
    Ptr<Node> dst = CreateObject<Node>();

    NodeContainer allNodes(src, n0, r, n1, dst);

    // Create devices and channels
    CsmaHelper csma5000;
    csma5000.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csma5000.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));
    csma5000.SetDeviceAttribute("Mtu", UintegerValue(5000));

    CsmaHelper csma1500;
    csma1500.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csma1500.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));
    csma1500.SetDeviceAttribute("Mtu", UintegerValue(1500));

    // Install CSMA devices
    NetDeviceContainer devSrcN0 = csma5000.Install(NodeContainer(src, n0));
    NetDeviceContainer devN0R = csma5000.Install(NodeContainer(n0, r));
    NetDeviceContainer devRN1 = csma1500.Install(NodeContainer(r, n1));
    NetDeviceContainer devN1Dst = csma1500.Install(NodeContainer(n1, dst));

    // Install Internet stacks
    InternetStackHelper internetv6;
    internetv6.SetIpv4StackEnabled(false);
    internetv6.SetIpv6StackEnabled(true);
    internetv6.Install(allNodes);

    // Assign IPv6 addresses
    Ipv6AddressHelper ipv6;

    ipv6.SetBase(Ipv6Address("2001:db8::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer iSrcN0 = ipv6.Assign(devSrcN0);
    iSrcN0.SetForwarding(1, true);
    iSrcN0.SetDefaultRouteInAllNodes(1);

    ipv6.NewNetwork();
    Ipv6InterfaceContainer iN0R = ipv6.Assign(devN0R);
    iN0R.SetForwarding(0, true);
    iN0R.SetForwarding(1, true);

    ipv6.NewNetwork();
    Ipv6InterfaceContainer iRN1 = ipv6.Assign(devRN1);
    iRN1.SetForwarding(0, true);
    iRN1.SetForwarding(1, true);

    ipv6.NewNetwork();
    Ipv6InterfaceContainer iN1Dst = ipv6.Assign(devN1Dst);
    iN1Dst.SetForwarding(1, true);
    iN1Dst.SetDefaultRouteInAllNodes(1);

    // Enable routing
    Ipv6StaticRoutingHelper routingHelper;
    Ptr<Ipv6StaticRouting> srN0 = routingHelper.GetStaticRouting(iN0R.GetIpv6(0));
    srN0->AddNetworkRouteTo(Ipv6Address("2001:db8:0:3::"), Ipv6Prefix(64), 2);

    Ptr<Ipv6StaticRouting> srR1 = routingHelper.GetStaticRouting(iRN1.GetIpv6(0));
    srR1->AddNetworkRouteTo(Ipv6Address("2001:db8:0:0::"), Ipv6Prefix(64), 1);
    srR1->AddNetworkRouteTo(Ipv6Address("2001:db8:0:2::"), Ipv6Prefix(64), 2);

    Ptr<Ipv6StaticRouting> srN1 = routingHelper.GetStaticRouting(iRN1.GetIpv6(1));
    srN1->AddNetworkRouteTo(Ipv6Address("2001:db8::"), Ipv6Prefix(64), 1);

    // Set up UDP Echo Server on Dst
    uint16_t port = 9;
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApp = echoServer.Install(dst);
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Set up UDP Echo Client on Src
    UdpEchoClientHelper echoClient(iN1Dst.GetAddress(1, 1), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(2000));

    ApplicationContainer clientApp = echoClient.Install(src);
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Enable tracing
    AsciiTraceHelper asciiTraceHelper;
    Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream("fragmentation-ipv6-two-mtu.tr");
    csma5000.EnableAsciiAll(stream);
    csma1500.EnableAsciiAll(stream);

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}