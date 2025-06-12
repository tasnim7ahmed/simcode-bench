#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv6-routing-table-entry.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("FragmentationIPv6PMTU");

int main(int argc, char *argv[]) {
    // Enable logging
    LogComponentEnable("FragmentationIPv6PMTU", LOG_LEVEL_INFO);
    LogComponentEnable("Ipv6L3Protocol", LOG_LEVEL_INFO);
    LogComponentEnable("Icmpv6L4Protocol", LOG_LEVEL_INFO);

    // Create nodes
    NodeContainer src, n0, r1, n1, r2, n2;
    src.Create(1);
    n0.Create(1);
    r1.Create(1);
    n1.Create(1);
    r2.Create(1);
    n2.Create(1);

    // Create channel containers
    PointToPointHelper p2p;
    CsmaHelper csma;

    // Set up links with different MTUs
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // Src -> n0 (MTU 5000)
    NetDeviceContainer devSrcToN0 = p2p.Install(NodeContainer(src, n0));
    for (auto dev : devSrcToN0) {
        dev->SetMtu(5000);
    }

    // n0 -> r1 (MTU 5000 via CSMA)
    csma.SetChannelAttribute("DataRate", StringValue("5Mbps"));
    csma.SetChannelAttribute("Delay", StringValue("2ms"));
    NetDeviceContainer devN0ToR1 = csma.Install(NodeContainer(n0, r1));
    for (auto dev : devN0ToR1) {
        dev->SetMtu(5000);
    }

    // r1 -> n1 (MTU 2000)
    NetDeviceContainer devR1ToN1 = p2p.Install(NodeContainer(r1, n1));
    for (auto dev : devR1ToN1) {
        dev->SetMtu(2000);
    }

    // n1 -> r2 (MTU 1500 via CSMA)
    NetDeviceContainer devN1ToR2 = csma.Install(NodeContainer(n1, r2));
    for (auto dev : devN1ToR2) {
        dev->SetMtu(1500);
    }

    // r2 -> n2 (MTU 1500)
    NetDeviceContainer devR2ToN2 = p2p.Install(NodeContainer(r2, n2));
    for (auto dev : devR2ToN2) {
        dev->SetMtu(1500);
    }

    // Install internet stack
    InternetStackHelper internetv6;
    internetv6.SetIpv4StackEnabled(false);
    internetv6.SetIpv6StackEnabled(true);
    internetv6.InstallAll();

    // Assign IPv6 addresses
    Ipv6AddressHelper ipv6;

    ipv6.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer ifSrcToN0 = ipv6.Assign(devSrcToN0);
    ifSrcToN0.SetForwarding(0, true);
    ifSrcToN0.SetDefaultRouteInAllNodes(0);

    ipv6.SetBase(Ipv6Address("2001:2::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer ifN0ToR1 = ipv6.Assign(devN0ToR1);
    ifN0ToR1.SetForwarding(0, true);
    ifN0ToR1.SetForwarding(1, true);

    ipv6.SetBase(Ipv6Address("2001:3::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer ifR1ToN1 = ipv6.Assign(devR1ToN1);
    ifR1ToN1.SetForwarding(0, true);
    ifR1ToN1.SetForwarding(1, true);

    ipv6.SetBase(Ipv6Address("2001:4::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer ifN1ToR2 = ipv6.Assign(devN1ToR2);
    ifN1ToR2.SetForwarding(0, true);
    ifN1ToR2.SetForwarding(1, true);

    ipv6.SetBase(Ipv6Address("2001:5::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer ifR2ToN2 = ipv6.Assign(devR2ToN2);
    ifR2ToN2.SetForwarding(0, true);
    ifR2ToN2.SetDefaultRouteInAllNodes(1);

    // Set up static routes
    Ptr<Ipv6> ipv6n0 = n0.Get(0)->GetObject<Ipv6>();
    Ptr<Ipv6RoutingTableEntry> route1 = Ipv6RoutingTableEntry::CreateNetworkRouteTo(
        Ipv6Address("2001:5::"), Ipv6Prefix(64), 2);
    ipv6n0->GetRoutingProtocol()->NotifyAddRoute(route1);

    Ptr<Ipv6> ipv6r1 = r1.Get(0)->GetObject<Ipv6>();
    ipv6r1->GetRoutingProtocol()->NotifyAddRoute(
        Ipv6RoutingTableEntry::CreateNetworkRouteTo(Ipv6Address("2001:5::"), Ipv6Prefix(64), 2));

    Ptr<Ipv6> ipv6n1 = n1.Get(0)->GetObject<Ipv6>();
    ipv6n1->GetRoutingProtocol()->NotifyAddRoute(
        Ipv6RoutingTableEntry::CreateNetworkRouteTo(Ipv6Address("2001:1::"), Ipv6Prefix(64), 0));
    ipv6n1->GetRoutingProtocol()->NotifyAddRoute(
        Ipv6RoutingTableEntry::CreateNetworkRouteTo(Ipv6Address("2001:2::"), Ipv6Prefix(64), 0));

    Ptr<Ipv6> ipv6r2 = r2.Get(0)->GetObject<Ipv6>();
    ipv6r2->GetRoutingProtocol()->NotifyAddRoute(
        Ipv6RoutingTableEntry::CreateNetworkRouteTo(Ipv6Address("2001:1::"), Ipv6Prefix(64), 0));
    ipv6r2->GetRoutingProtocol()->NotifyAddRoute(
        Ipv6RoutingTableEntry::CreateNetworkRouteTo(Ipv6Address("2001:2::"), Ipv6Prefix(64), 0));
    ipv6r2->GetRoutingProtocol()->NotifyAddRoute(
        Ipv6RoutingTableEntry::CreateNetworkRouteTo(Ipv6Address("2001:3::"), Ipv6Prefix(64), 0));

    // Install UDP Echo Server on n2
    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(n2);
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // Install UDP Echo Client on src, sending to n2's address
    UdpEchoClientHelper echoClient(ifR2ToN2.GetAddress(1), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(3000)); // Large enough to fragment

    ApplicationContainer clientApps = echoClient.Install(src);
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Configure tracing
    AsciiTraceHelper asciiTraceHelper;
    Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream("fragmentation-ipv6-PMTU.tr");
    p2p.EnableAsciiAll(stream);
    csma.EnableAsciiAll(stream);

    Simulator::Stop(Seconds(11.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}