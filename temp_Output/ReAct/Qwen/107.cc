#include "ns3/core-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv6-routing-table-entry.h"
#include "ns3/traffic-control-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("Ipv6FragmentationTwoMtu");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    NodeContainer srcNodes;
    srcNodes.Create(1); // Src
    NodeContainer n0 = srcNodes.Get(0)->GetObject<Node>();
    
    NodeContainer rNodes;
    rNodes.Create(1); // Router
    NodeContainer r = rNodes.Get(0)->GetObject<Node>();

    NodeContainer dstNodes;
    dstNodes.Create(1); // Dst
    NodeContainer n1 = dstNodes.Get(0)->GetObject<Node>();

    NodeContainer csmaNodesLeft;
    csmaNodesLeft.Add(n0);
    csmaNodesLeft.Add(r);

    CsmaHelper csmaLeft;
    csmaLeft.SetChannelAttribute("DataRate", DataRateValue(DataRate("100Mbps")));
    csmaLeft.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));
    NetDeviceContainer leftDevices = csmaLeft.Install(csmaNodesLeft);

    leftDevices.Get(0)->SetMtu(5000);
    leftDevices.Get(1)->SetMtu(5000);

    NodeContainer csmaNodesRight;
    csmaNodesRight.Add(r);
    csmaNodesRight.Add(n1);

    CsmaHelper csmaRight;
    csmaRight.SetChannelAttribute("DataRate", DataRateValue(DataRate("100Mbps")));
    csmaRight.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));
    NetDeviceContainer rightDevices = csmaRight.Install(csmaNodesRight);

    rightDevices.Get(0)->SetMtu(1500);
    rightDevices.Get(1)->SetMtu(1500);

    InternetStackHelper internetv6;
    internetv6.SetIpv4StackEnabled(false);
    internetv6.SetIpv6StackEnabled(true);
    internetv6.Install(srcNodes);
    internetv6.Install(rNodes);
    internetv6.Install(dstNodes);

    Ipv6AddressHelper ipv6Left;
    ipv6Left.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer leftInterfaces = ipv6Left.Assign(leftDevices);
    leftInterfaces.SetForwarding(0, true);
    leftInterfaces.SetDefaultRouteInAllNodes(0);
    leftInterfaces.SetForwarding(1, true);
    leftInterfaces.SetDefaultRouteInAllNodes(1);

    Ipv6AddressHelper ipv6Right;
    ipv6Right.SetBase(Ipv6Address("2001:2::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer rightInterfaces = ipv6Right.Assign(rightDevices);
    rightInterfaces.SetForwarding(0, true);
    rightInterfaces.SetDefaultRouteInAllNodes(0);
    rightInterfaces.SetForwarding(1, true);
    rightInterfaces.SetDefaultRouteInAllNodes(1);

    Ipv6StaticRoutingHelper ipv6RoutingHelper;
    Ptr<Ipv6> ipv6 = r->GetObject<Ipv6>();
    Ptr<Ipv6RoutingTableEntry> route1 = ipv6RoutingHelper.AddNetworkRouteTo(Ipv6Address("2001:1::"), Ipv6Prefix(64), 1, ipv6);
    Ptr<Ipv6RoutingTableEntry> route2 = ipv6RoutingHelper.AddNetworkRouteTo(Ipv6Address("2001:2::"), Ipv6Prefix(64), 2, ipv6);

    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(n1);
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient(rightInterfaces.GetAddress(1, 1), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(2000));
    ApplicationContainer clientApps = echoClient.Install(n0);
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    AsciiTraceHelper ascii;
    csmaLeft.EnableAsciiAll(ascii.CreateFileStream("fragmentation-ipv6-two-mtu.tr"));
    csmaRight.EnableAsciiAll(ascii.CreateFileStream("fragmentation-ipv6-two-mtu.tr"));

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}