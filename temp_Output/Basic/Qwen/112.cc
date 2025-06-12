#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/csma-module.h"
#include "ns3/ipv6-routing-table-entry.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("RadvdTwoPrefixSimulation");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("RadvdTwoPrefixSimulation", LOG_LEVEL_INFO);

    NodeContainer n0nR;
    n0nR.Create(2);
    Ptr<Node> n0 = n0nR.Get(0);
    Ptr<Node> router = n0nR.Get(1);

    NodeContainer n1 = CreateObject<Node>();
    NodeContainer rN1;
    rN1.Add(router);
    rN1.Add(n1);

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", DataRateValue(DataRate("5Mbps")));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));

    NetDeviceContainer d0dR = csma.Install(n0nR);
    NetDeviceContainer dRd1 = csma.Install(rN1);

    InternetStackHelper internetv6;
    internetv6.SetIpv4StackEnabled(false);
    internetv6.SetIpv6StackEnabled(true);
    internetv6.InstallAll();

    Ipv6AddressHelper ipv6;

    ipv6.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer i0iR;
    i0iR = ipv6.Assign(d0dR);
    i0iR.SetForwarding(1, true);
    i0iR.SetDefaultRouteInCaseOfAddAddress(1, true);

    ipv6.SetBase(Ipv6Address("2001:2::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer iRi1;
    iRi1 = ipv6.Assign(dRd1);
    iRi1.SetForwarding(0, true);
    iRi1.SetDefaultRouteInCaseOfAddAddress(0, true);

    Ipv6InterfaceAddress ifaceAddr1 = Ipv6InterfaceAddress(Ipv6Address("2001:ABCD::1"), Ipv6Prefix(64));
    n0->GetObject<Ipv6>()->GetInterface(1)->AddAddress(ifaceAddr1);

    Ipv6InterfaceAddress ifaceAddr2 = Ipv6InterfaceAddress(Ipv6Address("2001:ABCD::2"), Ipv6Prefix(64));
    router->GetObject<Ipv6>()->GetInterface(0)->AddAddress(ifaceAddr2);

    Ipv6StaticRoutingHelper routingHelper;
    Ptr<Ipv6RoutingProtocol> routingProto = routingHelper.GetStaticRouting(n0->GetObject<Ipv6>());
    routingProto->AddNetworkRouteTo(Ipv6Address("2001:2::"), Ipv6Prefix(64), 1);

    routingProto = routingHelper.GetStaticRouting(n1->GetObject<Ipv6>());
    routingProto->AddNetworkRouteTo(Ipv6Address("2001:1::"), Ipv6Prefix(64), 0);
    routingProto->AddNetworkRouteTo(Ipv6Address("2001:ABCD::"), Ipv6Prefix(64), 0);

    Ipv6ListRoutingHelper listRH;
    Ipv6StaticRoutingHelper* staticRh = new Ipv6StaticRoutingHelper();
    listRH.Add(staticRh, 0);
    RadvdHelper radvdHelper;
    ApplicationContainer radvdApps;

    radvdHelper.AddAnnouncedPrefix(router, 0, Ipv6Address("2001:1::"), 64);
    radvdHelper.AddAnnouncedPrefix(router, 0, Ipv6Address("2001:ABCD::"), 64);
    radvdHelper.AddAnnouncedPrefix(router, 1, Ipv6Address("2001:2::"), 64);

    radvdApps.Add(radvdHelper.Install(router));
    radvdApps.Start(Seconds(1.0));
    radvdApps.Stop(Seconds(10.0));

    V4PingHelper ping6(n1->GetObject<Ipv6>()->GetAddress(0, 0).GetAddress());
    ping6.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    ping6.SetAttribute("Size", UintegerValue(1024));
    ApplicationContainer apps = ping6.Install(n0);
    apps.Start(Seconds(2.0));
    apps.Stop(Seconds(9.0));

    AsciiTraceHelper ascii;
    csma.EnableAsciiAll(ascii.CreateFileStream("radvd-two-prefix.tr"));
    csma.EnablePcapAll("radvd-two-prefix");

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}