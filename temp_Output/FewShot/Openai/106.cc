#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv6-static-routing-helper.h"
#include <fstream>

using namespace ns3;

// Packet reception logging
static void
RxTrace(std::string context, Ptr<const Packet> packet, const Address &address)
{
    static std::ofstream outFile("fragmentation-ipv6-PMTU.tr", std::ios_base::app);
    outFile << Simulator::Now().GetSeconds()
            << "s " << context
            << " Packet received, size=" << packet->GetSize()
            << std::endl;
}

int main(int argc, char *argv[])
{
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Nodes: src, n0, r1, n1, r2, n2
    Ptr<Node> src = CreateObject<Node>();
    Ptr<Node> n0 = CreateObject<Node>();
    Ptr<Node> r1 = CreateObject<Node>();
    Ptr<Node> n1 = CreateObject<Node>();
    Ptr<Node> r2 = CreateObject<Node>();
    Ptr<Node> n2 = CreateObject<Node>();

    // Name nodes
    Names::Add("Src", src);
    Names::Add("n0", n0);
    Names::Add("r1", r1);
    Names::Add("n1", n1);
    Names::Add("r2", r2);
    Names::Add("n2", n2);

    // Segment 1: src <-> n0 via CSMA (MTU 5000)
    NodeContainer segment1;
    segment1.Add(src);
    segment1.Add(n0);

    CsmaHelper csma1;
    csma1.SetChannelAttribute("DataRate", DataRateValue(DataRate("10Mbps")));
    csma1.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));
    csma1.SetDeviceAttribute("Mtu", UintegerValue(5000));
    NetDeviceContainer dev_csma1 = csma1.Install(segment1);

    // Segment 2: n0 <-> r1 <-> n1 via PointToPoint (n0<->r1 MTU 2000, r1<->n1 MTU 2000)
    NodeContainer n0r1;
    n0r1.Add(n0);
    n0r1.Add(r1);

    PointToPointHelper p2p1;
    p2p1.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p1.SetChannelAttribute("Delay", StringValue("4ms"));
    p2p1.SetDeviceAttribute("Mtu", UintegerValue(2000));
    NetDeviceContainer dev_p2p1 = p2p1.Install(n0r1);

    NodeContainer r1n1;
    r1n1.Add(r1);
    r1n1.Add(n1);

    PointToPointHelper p2p2;
    p2p2.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p2.SetChannelAttribute("Delay", StringValue("4ms"));
    p2p2.SetDeviceAttribute("Mtu", UintegerValue(2000));
    NetDeviceContainer dev_p2p2 = p2p2.Install(r1n1);

    // Segment 3: n1 <-> r2 <-> n2 via PointToPoint (n1<->r2 MTU 1500, r2<->n2 MTU 1500)
    NodeContainer n1r2;
    n1r2.Add(n1);
    n1r2.Add(r2);

    PointToPointHelper p2p3;
    p2p3.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p3.SetChannelAttribute("Delay", StringValue("4ms"));
    p2p3.SetDeviceAttribute("Mtu", UintegerValue(1500));
    NetDeviceContainer dev_p2p3 = p2p3.Install(n1r2);

    NodeContainer r2n2;
    r2n2.Add(r2);
    r2n2.Add(n2);

    PointToPointHelper p2p4;
    p2p4.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p4.SetChannelAttribute("Delay", StringValue("4ms"));
    p2p4.SetDeviceAttribute("Mtu", UintegerValue(1500));
    NetDeviceContainer dev_p2p4 = p2p4.Install(r2n2);

    // Install IPv6 stack
    InternetStackHelper stack;
    stack.Install(src);
    stack.Install(n0);
    stack.Install(r1);
    stack.Install(n1);
    stack.Install(r2);
    stack.Install(n2);

    // Assign IPv6 addresses
    Ipv6AddressHelper ipv6;

    // Segment 1: src-n0
    ipv6.SetBase("2001:1::", 64);
    Ipv6InterfaceContainer iface_csma1 = ipv6.Assign(dev_csma1);
    iface_csma1.SetForwarding(0, true);
    iface_csma1.SetForwarding(1, true);

    // Segment 2: n0-r1 (p2p1)
    ipv6.SetBase("2001:2::", 64);
    Ipv6InterfaceContainer iface_p2p1 = ipv6.Assign(dev_p2p1);
    iface_p2p1.SetForwarding(0, true);
    iface_p2p1.SetForwarding(1, true);

    // Segment 2: r1-n1 (p2p2)
    ipv6.SetBase("2001:3::", 64);
    Ipv6InterfaceContainer iface_p2p2 = ipv6.Assign(dev_p2p2);
    iface_p2p2.SetForwarding(0, true);
    iface_p2p2.SetForwarding(1, true);

    // Segment 3: n1-r2 (p2p3)
    ipv6.SetBase("2001:4::", 64);
    Ipv6InterfaceContainer iface_p2p3 = ipv6.Assign(dev_p2p3);
    iface_p2p3.SetForwarding(0, true);
    iface_p2p3.SetForwarding(1, true);

    // Segment 3: r2-n2 (p2p4)
    ipv6.SetBase("2001:5::", 64);
    Ipv6InterfaceContainer iface_p2p4 = ipv6.Assign(dev_p2p4);
    iface_p2p4.SetForwarding(0, true);
    iface_p2p4.SetForwarding(1, true);

    // Enable forwarding on routers
    Ptr<Ipv6> ipv6_r1 = r1->GetObject<Ipv6>();
    for (uint32_t i = 0; i < ipv6_r1->GetNInterfaces(); ++i)
        ipv6_r1->SetForwarding(i, true);

    Ptr<Ipv6> ipv6_r2 = r2->GetObject<Ipv6>();
    for (uint32_t i = 0; i < ipv6_r2->GetNInterfaces(); ++i)
        ipv6_r2->SetForwarding(i, true);

    // Set default routes
    // Src default route via n0
    Ipv6StaticRoutingHelper ipv6RoutingHelper;
    Ptr<Ipv6StaticRouting> srcStaticRouting = ipv6RoutingHelper.GetStaticRouting(src->GetObject<Ipv6>());
    srcStaticRouting->SetDefaultRoute(iface_csma1.GetAddress(1,1), 1);

    // n0 default route via r1
    Ptr<Ipv6StaticRouting> n0StaticRouting = ipv6RoutingHelper.GetStaticRouting(n0->GetObject<Ipv6>());
    n0StaticRouting->SetDefaultRoute(iface_p2p1.GetAddress(1,1), 2);

    // n1 default route via r2
    Ptr<Ipv6StaticRouting> n1StaticRouting = ipv6RoutingHelper.GetStaticRouting(n1->GetObject<Ipv6>());
    n1StaticRouting->SetDefaultRoute(iface_p2p3.GetAddress(1,1), 2);

    // n2 default route via r2
    Ptr<Ipv6StaticRouting> n2StaticRouting = ipv6RoutingHelper.GetStaticRouting(n2->GetObject<Ipv6>());
    n2StaticRouting->SetDefaultRoute(iface_p2p4.GetAddress(0,1), 1);

    // r1 static routing: n0<->n1
    Ptr<Ipv6StaticRouting> r1StaticRouting = ipv6RoutingHelper.GetStaticRouting(r1->GetObject<Ipv6>());
    r1StaticRouting->AddNetworkRouteTo(Ipv6Address("2001:1::"), Ipv6Prefix(64), iface_p2p1.GetAddress(0,1), 1);
    r1StaticRouting->AddNetworkRouteTo(Ipv6Address("2001:5::"), Ipv6Prefix(64), iface_p2p2.GetAddress(1,1), 2);
    r1StaticRouting->AddNetworkRouteTo(Ipv6Address("2001:4::"), Ipv6Prefix(64), iface_p2p2.GetAddress(1,1), 2);

    // r2 static routing: n1<->n2
    Ptr<Ipv6StaticRouting> r2StaticRouting = ipv6RoutingHelper.GetStaticRouting(r2->GetObject<Ipv6>());
    r2StaticRouting->AddNetworkRouteTo(Ipv6Address("2001:3::"), Ipv6Prefix(64), iface_p2p3.GetAddress(0,1), 1);
    r2StaticRouting->AddNetworkRouteTo(Ipv6Address("2001:2::"), Ipv6Prefix(64), iface_p2p3.GetAddress(0,1), 1);
    r2StaticRouting->AddNetworkRouteTo(Ipv6Address("2001:1::"), Ipv6Prefix(64), iface_p2p3.GetAddress(0,1), 1);

    // UDP Echo server on n2, client on src
    uint16_t port = 9;
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApps = echoServer.Install(n2);
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(16.0));

    // Echo client at src, sending > 5000 bytes to force fragmentation at r1 and r2
    UdpEchoClientHelper echoClient(iface_p2p4.GetAddress(1,1), port); // n2's interface

    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(6000)); // Larger than any MTU

    ApplicationContainer clientApps = echoClient.Install(src);
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(15.0));

    // Tracing: Packet sink at n2
    Ptr<Node> nodeN2 = n2;
    Ptr<Ipv6> ipv6N2 = nodeN2->GetObject<Ipv6>();
    for (uint32_t i = 0; i < nodeN2->GetNDevices(); ++i)
    {
        std::ostringstream oss;
        oss << "/NodeList/" << nodeN2->GetId() << "/DeviceList/" << i << "/$ns3::NetDevice/MacRx";
        Config::Connect(oss.str(), MakeCallback(&RxTrace));
    }

    Simulator::Stop(Seconds(16.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}