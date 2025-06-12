#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv6-static-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("FragmentationIpv6PMTU");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
    LogComponentEnable("FragmentationIpv6PMTU", LOG_LEVEL_INFO);

    NodeContainer nodes;
    nodes.Create(6); // Src, n0, r1, n1, r2, n2

    NodeContainer p2pLink1(nodes.Get(0), nodes.Get(1)); // Src <-> n0
    NodeContainer csmaLink1(nodes.Get(1), nodes.Get(2)); // n0 <-> r1
    NodeContainer p2pLink2(nodes.Get(2), nodes.Get(3)); // r1 <-> n1
    NodeContainer csmaLink2(nodes.Get(3), nodes.Get(4)); // n1 <-> r2
    NodeContainer p2pLink3(nodes.Get(4), nodes.Get(5)); // r2 <-> n2

    PointToPointHelper p2p;
    CsmaHelper csma;

    // MTU settings: 5000, 2000, 1500
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));
    csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay", StringValue("6560ns"));

    NetDeviceContainer devSrc_n0 = p2p.Install(p2pLink1);
    NetDeviceContainer devn0_r1 = csma.Install(csmaLink1);
    NetDeviceContainer devr1_n1 = p2p.Install(p2pLink2);
    NetDeviceContainer devn1_r2 = csma.Install(csmaLink2);
    NetDeviceContainer devr2_n2 = p2p.Install(p2pLink3);

    // Set MTUs accordingly
    for (uint32_t i = 0; i < devSrc_n0.GetN(); ++i) {
        Ptr<NetDevice> device = devSrc_n0.Get(i);
        device->SetMtu(5000);
    }

    for (uint32_t i = 0; i < devn0_r1.GetN(); ++i) {
        Ptr<NetDevice> device = devn0_r1.Get(i);
        device->SetMtu(5000);
    }

    for (uint32_t i = 0; i < devr1_n1.GetN(); ++i) {
        Ptr<NetDevice> device = devr1_n1.Get(i);
        device->SetMtu(2000);
    }

    for (uint32_t i = 0; i < devn1_r2.GetN(); ++i) {
        Ptr<NetDevice> device = devn1_r2.Get(i);
        device->SetMtu(2000);
    }

    for (uint32_t i = 0; i < devr2_n2.GetN(); ++i) {
        Ptr<NetDevice> device = devr2_n2.Get(i);
        device->SetMtu(1500);
    }

    InternetStackHelper internetv6;
    internetv6.SetIpv4StackEnabled(false);
    internetv6.SetIpv6StackEnabled(true);
    internetv6.Install(nodes);

    Ipv6AddressHelper ipv6;

    ipv6.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer ifSrc_n0 = ipv6.Assign(devSrc_n0);
    ifSrc_n0.SetForwarding(0, true);
    ifSrc_n0.SetDefaultRouteInAllNodes();

    Ipv6InterfaceContainer ifn0_r1 = ipv6.Assign(devn0_r1);
    ifn0_r1.SetForwarding(0, true);
    ifn0_r1.SetForwarding(1, true);

    Ipv6InterfaceContainer ifr1_n1 = ipv6.Assign(devr1_n1);
    ifr1_n1.SetForwarding(0, true);
    ifr1_n1.SetForwarding(1, true);

    Ipv6InterfaceContainer ifn1_r2 = ipv6.Assign(devn1_r2);
    ifn1_r2.SetForwarding(0, true);
    ifn1_r2.SetForwarding(1, true);

    Ipv6InterfaceContainer ifr2_n2 = ipv6.Assign(devr2_n2);
    ifr2_n2.SetForwarding(1, true);

    Ipv6StaticRoutingHelper routingHelper;

    Ptr<Ipv6> ipv6n0 = nodes.Get(1)->GetObject<Ipv6>();
    Ptr<Ipv6RoutingProtocol> rpv6n0 = routingHelper.GetStaticRouting(ipv6n0);
    rpv6n0->AddNetworkRouteTo(Ipv6Address("2001:3::"), Ipv6Prefix(64), 2, 0);

    Ptr<Ipv6> ipv6r1 = nodes.Get(2)->GetObject<Ipv6>();
    Ptr<Ipv6RoutingProtocol> rpv6r1 = routingHelper.GetStaticRouting(ipv6r1);
    rpv6r1->AddNetworkRouteTo(Ipv6Address("2001:1::"), Ipv6Prefix(64), 1, 0);
    rpv6r1->AddNetworkRouteTo(Ipv6Address("2001:4::"), Ipv6Prefix(64), 2, 0);

    Ptr<Ipv6> ipv6n1 = nodes.Get(3)->GetObject<Ipv6>();
    Ptr<Ipv6RoutingProtocol> rpv6n1 = routingHelper.GetStaticRouting(ipv6n1);
    rpv6n1->AddNetworkRouteTo(Ipv6Address("2001:2::"), Ipv6Prefix(64), 1, 0);
    rpv6n1->AddNetworkRouteTo(Ipv6Address("2001:5::"), Ipv6Prefix(64), 2, 0);

    Ptr<Ipv6> ipv6r2 = nodes.Get(4)->GetObject<Ipv6>();
    Ptr<Ipv6RoutingProtocol> rpv6r2 = routingHelper.GetStaticRouting(ipv6r2);
    rpv6r2->AddNetworkRouteTo(Ipv6Address("2001:3::"), Ipv6Prefix(64), 1, 0);

    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(5));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient(ifr2_n2.GetAddress(1, 1), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.)));
    echoClient.SetAttribute("PacketSize", UintegerValue(3000));

    ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    AsciiTraceHelper ascii;
    Ptr<OutputStreamWrapper> stream = ascii.CreateFileStream("fragmentation-ipv6-PMTU.tr");
    p2p.EnableAsciiAll(stream);
    csma.EnableAsciiAll(stream);

    Simulator::Run();
    Simulator::Destroy();
    return 0;
}