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
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    NodeContainer src;
    src.Create(1);
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

    NodeContainer csmaNodes;
    csmaNodes.Add(n0);
    csmaNodes.Add(r1);
    csmaNodes.Add(n1);
    csmaNodes.Add(r2);
    csmaNodes.Add(n2);

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", DataRateValue(DataRate("100Mbps")));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));
    csma.SetDeviceAttribute("Mtu", UintegerValue(5000));
    NetDeviceContainer csmaDevices = csma.Install(csmaNodes);

    PointToPointHelper p2pSrcToN0;
    p2pSrcToN0.SetDeviceAttribute("DataRate", DataRateValue(DataRate("5Mbps")));
    p2pSrcToN0.SetChannelAttribute("Delay", TimeValue(MilliSeconds(10)));
    p2pSrcToN0.SetDeviceAttribute("Mtu", UintegerValue(2000));
    NetDeviceContainer p2pSrcToN0Devices = p2pSrcToN0.Install(src, n0);

    PointToPointHelper p2pN1ToR2;
    p2pN1ToR2.SetDeviceAttribute("DataRate", DataRateValue(DataRate("2Mbps")));
    p2pN1ToR2.SetChannelAttribute("Delay", TimeValue(MilliSeconds(5)));
    p2pN1ToR2.SetDeviceAttribute("Mtu", UintegerValue(1500));
    NetDeviceContainer p2pN1ToR2Devices = p2pN1ToR2.Install(n1, r2);

    InternetStackHelper internetv6;
    internetv6.SetIpv4StackEnabled(false);
    internetv6.SetIpv6StackEnabled(true);
    internetv6.InstallAll();

    Ipv6AddressHelper ipv6;
    ipv6.SetBase(Ipv6Address("2001:db00::"), Ipv6Prefix(64));

    Ipv6InterfaceContainer csmaInterfaces = ipv6.Assign(csmaDevices);
    csmaInterfaces.SetForwarding(0, true);
    csmaInterfaces.SetDefaultRouteInAllNodes(0);

    ipv6.NewNetwork();
    Ipv6InterfaceContainer p2pSrcToN0Interfaces = ipv6.Assign(p2pSrcToN0Devices);
    p2pSrcToN0Interfaces.SetForwarding(1, true); // router n0

    ipv6.NewNetwork();
    Ipv6InterfaceContainer p2pN1ToR2Interfaces = ipv6.Assign(p2pN1ToR2Devices);
    p2pN1ToR2Interfaces.SetForwarding(1, true); // router r2

    Ipv6StaticRoutingHelper routingHelper;

    // From Src to n0
    Ptr<Ipv6StaticRouting> srcRouting = routingHelper.GetStaticRouting(src.Get(0)->GetObject<Ipv6>());
    srcRouting->AddHostRouteTo(Ipv6Address("2001:db01::2"), Ipv6Address("2001:db00::1"), 1);

    // On r1 (n0) add route for n1 and beyond through n0's interface to r1
    Ptr<Ipv6StaticRouting> r1Routing = routingHelper.GetStaticRouting(r1.Get(0)->GetObject<Ipv6>());
    r1Routing->AddHostRouteTo(Ipv6Address("2001:db02::2"), csmaInterfaces.GetAddress(2), 0);
    r1Routing->AddHostRouteTo(Ipv6Address("2001:db03::1"), csmaInterfaces.GetAddress(2), 0);

    // On r2, route back to source via its network
    Ptr<Ipv6StaticRouting> r2Routing = routingHelper.GetStaticRouting(r2.Get(0)->GetObject<Ipv6>());
    r2Routing->AddHostRouteTo(Ipv6Address("2001:db00::1"), p2pN1ToR2Interfaces.GetAddress(1), 1);
    r2Routing->AddHostRouteTo(Ipv6Address("2001:db01::1"), p2pN1ToR2Interfaces.GetAddress(1), 1);

    // UDP Echo Server on n2
    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(n2.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // UDP Echo Client on Src, sending to n2
    UdpEchoClientHelper echoClient(p2pN1ToR2Interfaces.GetAddress(1), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(4000));
    ApplicationContainer clientApps = echoClient.Install(src.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    AsciiTraceHelper ascii;
    Ptr<OutputStreamWrapper> stream = ascii.CreateFileStream("fragmentation-ipv6-PMTU.tr");
    csma.EnableAsciiIpv6All(stream);
    p2pSrcToN0.EnableAsciiIpv6All(stream);
    p2pN1ToR2.EnableAsciiIpv6All(stream);

    Simulator::Run();
    Simulator::Destroy();
    return 0;
}