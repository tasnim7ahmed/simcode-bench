#include "ns3/core-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/network-module.h"
#include "ns3/ipv6-static-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("FragmentationIpv6TwoMtuSimulation");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    NodeContainer nodes;
    nodes.Create(5);

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", DataRateValue(DataRate(5000000)));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));

    NetDeviceContainer devices0r;
    NetDeviceContainer devices1r;
    Ptr<Node> r = nodes.Get(2); // Router node

    // Src connected to n0
    NodeContainer srcN0(nodes.Get(0), nodes.Get(1));
    NetDeviceContainer srcN0Devs = csma.Install(srcN0);

    // n0 connected to router
    NodeContainer n0r(nodes.Get(1), r);
    devices0r = csma.Install(n0r);
    for (auto devIt = devices0r.Begin(); devIt != devices0r.End(); ++devIt) {
        DynamicCast<CsmaNetDevice>(*devIt)->SetMtu(5000);
    }

    // router connected to n1
    NodeContainer nr1(r, nodes.Get(3));
    devices1r = csma.Install(nr1);
    for (auto devIt = devices1r.Begin(); devIt != devices1r.End(); ++devIt) {
        DynamicCast<CsmaNetDevice>(*devIt)->SetMtu(1500);
    }

    // Dst connected to n1
    NodeContainer n1Dst(nodes.Get(3), nodes.Get(4));
    NetDeviceContainer n1DstDevs = csma.Install(n1Dst);

    InternetStackHelper stack;
    Ipv6AddressHelper address;

    stack.SetIpv4StackInstall(false);
    stack.SetIpv6StackInstall(true);
    stack.Install(nodes);

    // Assign IPv6 addresses
    address.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer interfacesSrcN0 = address.Assign(srcN0Devs);
    address.NewNetwork();
    Ipv6InterfaceContainer interfaces0r = address.Assign(devices0r);
    address.NewNetwork();
    Ipv6InterfaceContainer interfaces1r = address.Assign(devices1r);
    address.NewNetwork();
    Ipv6InterfaceContainer interfacesN1Dst = address.Assign(n1DstDevs);

    // Enable routing on the router
    Ipv6StaticRoutingHelper routingHelper;
    Ptr<Ipv6StaticRouting> rtr0StaticRouting = routingHelper.GetStaticRouting(r->GetObject<Ipv6>());
    rtr0StaticRouting->AddNetworkRouteTo(Ipv6Address("2001:1::"), Ipv6Prefix(64), 1);
    rtr0StaticRouting->AddNetworkRouteTo(Ipv6Address("2001:2::"), Ipv6Prefix(64), 2);
    rtr0StaticRouting->AddNetworkRouteTo(Ipv6Address("2001:3::"), Ipv6Prefix(64), 3);
    rtr0StaticRouting->AddNetworkRouteTo(Ipv6Address("2001:4::"), Ipv6Prefix(64), 3);

    // Server on Dst
    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(4));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // Client on Src sending to Dst's IPv6 address
    UdpEchoClientHelper echoClient(interfacesN1Dst.GetAddress(1, 1), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(2000));

    ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    AsciiTraceHelper traceHelper;
    Ptr<OutputStreamWrapper> stream = traceHelper.CreateFileStream("fragmentation-ipv6-two-mtu.tr");
    csma.EnableAsciiAll(stream);

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}