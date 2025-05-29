#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/ripng-module.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/ipv6-routing-table-entry.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/point-to-point-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    CommandLine cmd;
    cmd.Parse(argc, argv);

    NodeContainer src, a, b, c, d, dst;
    src.Create(1);
    a.Create(1);
    b.Create(1);
    c.Create(1);
    d.Create(1);
    dst.Create(1);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer srcA = p2p.Install(src.Get(0), a.Get(0));
    NetDeviceContainer aB = p2p.Install(a.Get(0), b.Get(0));
    NetDeviceContainer aC = p2p.Install(a.Get(0), c.Get(0));
    NetDeviceContainer bC = p2p.Install(b.Get(0), c.Get(0));
    NetDeviceContainer bD = p2p.Install(b.Get(0), d.Get(0));

    PointToPointHelper p2p_cd;
    p2p_cd.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p_cd.SetChannelAttribute("Delay", StringValue("2ms"));
    NetDeviceContainer cD = p2p_cd.Install(c.Get(0), d.Get(0));

    PointToPointHelper p2p_dst;
    p2p_dst.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p_dst.SetChannelAttribute("Delay", StringValue("2ms"));
    NetDeviceContainer dDst = p2p_dst.Install(d.Get(0), dst.Get(0));

    InternetStackHelper internet;
    RipNgHelper ripng;
    internet.Install(src);
    internet.Install(a);
    internet.Install(b);
    internet.Install(c);
    internet.Install(d);
    internet.Install(dst);

    Ipv6AddressHelper ipv6;
    ipv6.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer iSrcA = ipv6.Assign(srcA);
    Ipv6InterfaceContainer iAB = ipv6.Assign(aB);
    Ipv6InterfaceContainer iAC = ipv6.Assign(aC);
    Ipv6InterfaceContainer iBC = ipv6.Assign(bC);
    Ipv6InterfaceContainer iBD = ipv6.Assign(bD);
    Ipv6InterfaceContainer iCD = ipv6.Assign(cD);
    Ipv6InterfaceContainer iDDst = ipv6.Assign(dDst);

    iSrcA.GetAddress(0).SetScope(Ipv6Address::SCOPE_LINK_LOCAL);
    iAB.GetAddress(0).SetScope(Ipv6Address::SCOPE_LINK_LOCAL);
    iAC.GetAddress(0).SetScope(Ipv6Address::SCOPE_LINK_LOCAL);
    iBC.GetAddress(0).SetScope(Ipv6Address::SCOPE_LINK_LOCAL);
    iBD.GetAddress(0).SetScope(Ipv6Address::SCOPE_LINK_LOCAL);
    iCD.GetAddress(0).SetScope(Ipv6Address::SCOPE_LINK_LOCAL);
    iDDst.GetAddress(0).SetScope(Ipv6Address::SCOPE_LINK_LOCAL);

    iSrcA.GetAddress(1).SetScope(Ipv6Address::SCOPE_LINK_LOCAL);
    iAB.GetAddress(1).SetScope(Ipv6Address::SCOPE_LINK_LOCAL);
    iAC.GetAddress(1).SetScope(Ipv6Address::SCOPE_LINK_LOCAL);
    iBC.GetAddress(1).SetScope(Ipv6Address::SCOPE_LINK_LOCAL);
    iBD.GetAddress(1).SetScope(Ipv6Address::SCOPE_LINK_LOCAL);
    iCD.GetAddress(1).SetScope(Ipv6Address::SCOPE_LINK_LOCAL);
    iDDst.GetAddress(1).SetScope(Ipv6Address::SCOPE_LINK_LOCAL);

    iSrcA.Get(0)->SetAttribute("UseRouterSolicitation", BooleanValue(false));
    iAB.Get(0)->SetAttribute("UseRouterSolicitation", BooleanValue(false));
    iAC.Get(0)->SetAttribute("UseRouterSolicitation", BooleanValue(false));
    iBC.Get(0)->SetAttribute("UseRouterSolicitation", BooleanValue(false));
    iBD.Get(0)->SetAttribute("UseRouterSolicitation", BooleanValue(false));
    iCD.Get(0)->SetAttribute("UseRouterSolicitation", BooleanValue(false));
    iDDst.Get(0)->SetAttribute("UseRouterSolicitation", BooleanValue(false));

    iSrcA.Get(1)->SetAttribute("UseRouterSolicitation", BooleanValue(false));
    iAB.Get(1)->SetAttribute("UseRouterSolicitation", BooleanValue(false));
    iAC.Get(1)->SetAttribute("UseRouterSolicitation", BooleanValue(false));
    iBC.Get(1)->SetAttribute("UseRouterSolicitation", BooleanValue(false));
    iBD.Get(1)->SetAttribute("UseRouterSolicitation", BooleanValue(false));
    iCD.Get(1)->SetAttribute("UseRouterSolicitation", BooleanValue(false));
    iDDst.Get(1)->SetAttribute("UseRouterSolicitation", BooleanValue(false));

    Ipv6StaticRoutingHelper staticRouting;
    Ptr<Ipv6StaticRouting> routingA = staticRouting.GetStaticRouting(a.Get(0)->GetObject<Ipv6>()->GetRoutingProtocol());
    routingA->SetDefaultRoute(iAB.GetAddress(1), 0);

    Ptr<Ipv6StaticRouting> routingD = staticRouting.GetStaticRouting(d.Get(0)->GetObject<Ipv6>()->GetRoutingProtocol());
    routingD->SetDefaultRoute(iDDst.GetAddress(1), 0);

    ripng.SetSplitHorizon(true);
    ripng.SetMetric(iCD.GetAddress(0), 10);

    Address sinkLocalAddress(Inet6SocketAddress(iDDst.GetAddress(1), 80));
    PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", sinkLocalAddress);
    ApplicationContainer sinkApp = sinkHelper.Install(dst.Get(0));
    sinkApp.Start(Seconds(3.0));

    V6PingHelper ping(iDDst.GetAddress(1));
    ping.SetAttribute("Verbose", BooleanValue(true));
    ApplicationContainer pingApps = ping.Install(src.Get(0));
    pingApps.Start(Seconds(3.0));

    Simulator::Schedule(Seconds(40.0), [&]() {
        p2p.SetDeviceAttribute("DataRate", StringValue("0bps"));
    });

    Simulator::Schedule(Seconds(44.0), [&]() {
        p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    });

    ripng.Install(src.Get(0));
    ripng.Install(a.Get(0));
    ripng.Install(b.Get(0));
    ripng.Install(c.Get(0));
    ripng.Install(d.Get(0));
    ripng.Install(dst.Get(0));

    Simulator::Stop(Seconds(100.0));

    p2p.EnablePcapAll("ripng");
    AsciiTraceHelper ascii;
    p2p.EnableAsciiAll(ascii.CreateFileStream("ripng.tr"));

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}