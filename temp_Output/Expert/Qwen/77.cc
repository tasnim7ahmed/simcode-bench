#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv6-routing-table-entry.h"
#include "ns3/ipv6-routing-helper.h"
#include "ns3/ripng-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("RipNgSplitHorizon");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create nodes
    NodeContainer src;
    src.Create(1);
    NodeContainer a;
    a.Create(1);
    NodeContainer b;
    b.Create(1);
    NodeContainer c;
    c.Create(1);
    NodeContainer d;
    d.Create(1);
    NodeContainer dst;
    dst.Create(1);

    NodeContainer routerNodes = NodeContainer(src.Get(0), a.Get(0), b.Get(0), c.Get(0), d.Get(0), dst.Get(0));

    // Create links
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devSrcA = p2p.Install(src, a);
    NetDeviceContainer devAB = p2p.Install(a, b);
    NetDeviceContainer devAC = p2p.Install(a, c);
    NetDeviceContainer devBC = p2p.Install(b, c);
    NetDeviceContainer devBD = p2p.Install(b, d);
    NetDeviceContainer devCD = p2p.Install(c, d);
    NetDeviceContainer devDDst = p2p.Install(d, dst);

    InternetStackHelper internetv6;
    RipNgHelper ripNgRouting;

    ripNgRouting.Set("SplitHorizon", EnumValue(RipNg::SPLIT_HORIZON));

    Ipv6ListRoutingHelper listRH;
    listRH.Add(ripNgRouting, 0);

    internetv6.SetRoutingHelper(listRH);
    internetv6.Install(routerNodes);

    Ipv6AddressHelper ipv6;

    ipv6.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer ifSrcA = ipv6.Assign(devSrcA);
    ifSrcA.SetForwarding(0, true);
    ifSrcA.SetDefaultRouteInAllNodes(0);

    ipv6.NewNetwork();
    Ipv6InterfaceContainer ifAB = ipv6.Assign(devAB);
    ifAB.SetForwarding(0, true);
    ifAB.SetForwarding(1, true);

    ipv6.NewNetwork();
    Ipv6InterfaceContainer ifAC = ipv6.Assign(devAC);
    ifAC.SetForwarding(0, true);
    ifAC.SetForwarding(1, true);

    ipv6.NewNetwork();
    Ipv6InterfaceContainer ifBC = ipv6.Assign(devBC);
    ifBC.SetForwarding(0, true);
    ifBC.SetForwarding(1, true);

    ipv6.NewNetwork();
    Ipv6InterfaceContainer ifBD = ipv6.Assign(devBD);
    ifBD.SetForwarding(0, true);
    ifBD.SetForwarding(1, true);

    ipv6.NewNetwork();
    Ipv6InterfaceContainer ifCD = ipv6.Assign(devCD);
    ifCD.SetForwarding(0, true);
    ifCD.SetForwarding(1, true);

    ipv6.NewNetwork();
    Ipv6InterfaceContainer ifDDst = ipv6.Assign(devDDst);
    ifDDst.SetForwarding(0, true);
    ifDDst.SetDefaultRouteInAllNodes(1);

    // Assign static addresses to A and D
    Ptr<Node> nodeA = a.Get(0);
    Ptr<Ipv6> ipv6A = nodeA->GetObject<Ipv6>();
    Ipv6InterfaceAddress ifAddrA = Ipv6InterfaceAddress(Ipv6Address("2001:a::1"), Ipv6Prefix(64));
    uint32_t indexA = ipv6A->AddInterface(devAB.Get(0));
    ipv6A->AddAddress(indexA, ifAddrA);
    ipv6A->SetForwarding(indexA, true);
    ipv6A->SetUp(indexA);

    Ptr<Node> nodeD = d.Get(0);
    Ptr<Ipv6> ipv6D = nodeD->GetObject<Ipv6>();
    Ipv6InterfaceAddress ifAddrD = Ipv6InterfaceAddress(Ipv6Address("2001:d::1"), Ipv6Prefix(64));
    uint32_t indexD = ipv6D->AddInterface(devBD.Get(1));
    ipv6D->AddAddress(indexD, ifAddrD);
    ipv6D->SetForwarding(indexD, true);
    ipv6D->SetUp(indexD);

    // Set link metrics (costs)
    ripNgRouting.ExcludeLink(devCD.Get(0)->GetIfIndex());
    ripNgRouting.ExcludeLink(devCD.Get(1)->GetIfIndex());

    ripNgRouting.SetLinkMetric(devSrcA.Get(0)->GetIfIndex(), 1);
    ripNgRouting.SetLinkMetric(devSrcA.Get(1)->GetIfIndex(), 1);

    ripNgRouting.SetLinkMetric(devAB.Get(0)->GetIfIndex(), 1);
    ripNgRouting.SetLinkMetric(devAB.Get(1)->GetIfIndex(), 1);

    ripNgRouting.SetLinkMetric(devAC.Get(0)->GetIfIndex(), 1);
    ripNgRouting.SetLinkMetric(devAC.Get(1)->GetIfIndex(), 1);

    ripNgRouting.SetLinkMetric(devBC.Get(0)->GetIfIndex(), 1);
    ripNgRouting.SetLinkMetric(devBC.Get(1)->GetIfIndex(), 1);

    ripNgRouting.SetLinkMetric(devBD.Get(0)->GetIfIndex(), 1);
    ripNgRouting.SetLinkMetric(devBD.Get(1)->GetIfIndex(), 1);

    ripNgRouting.SetLinkMetric(devCD.Get(0)->GetIfIndex(), 10);
    ripNgRouting.SetLinkMetric(devCD.Get(1)->GetIfIndex(), 10);

    ripNgRouting.SetLinkMetric(devDDst.Get(0)->GetIfIndex(), 1);
    ripNgRouting.SetLinkMetric(devDDst.Get(1)->GetIfIndex(), 1);

    // Applications
    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(dst.Get(0));
    serverApps.Start(Seconds(2.0));
    serverApps.Stop(Seconds(50.0));

    UdpEchoClientHelper echoClient(ifDDst.GetAddress(1, 1), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(50));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = echoClient.Install(src.Get(0));
    clientApps.Start(Seconds(3.0));
    clientApps.Stop(Seconds(50.0));

    // Tracing
    AsciiTraceHelper ascii;
    p2p.EnableAsciiAll(ascii.CreateFileStream("ripng-split-horizon.tr"));
    p2p.EnablePcapAll("ripng-split-horizon");

    // Schedule link failure and recovery
    Simulator::Schedule(Seconds(40.0), &Ipv6::SetDown, ipv6D, devBD.Get(1)->GetIfIndex());
    Simulator::Schedule(Seconds(44.0), &Ipv6::SetUp, ipv6D, devBD.Get(1)->GetIfIndex());

    Simulator::Stop(Seconds(50.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}