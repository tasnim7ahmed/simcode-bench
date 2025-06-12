#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/csma-module.h"
#include "ns3/ipv6-routing-table-entry.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("RadvdSimulation");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    NodeContainer nodes;
    nodes.Create(2);

    NodeContainer router;
    router.Create(1);

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", DataRateValue(DataRate(5000000)));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));

    NetDeviceContainer ndc0 = csma.Install(NodeContainer(nodes.Get(0), router.Get(0)));
    NetDeviceContainer ndc1 = csma.Install(NodeContainer(nodes.Get(1), router.Get(0)));

    InternetStackHelper internetv6;
    internetv6.SetIpv4StackEnabled(false);
    internetv6.SetIpv6StackEnabled(true);
    internetv6.Install(nodes);
    internetv6.Install(router);

    Ipv6AddressHelper ipv6;

    ipv6.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer iic0 = ipv6.Assign(ndc0);
    iic0.SetForwarding(0, true);
    iic0.SetDefaultRouteInAllNodes(0);

    ipv6.SetBase(Ipv6Address("2001:2::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer iic1 = ipv6.Assign(ndc1);
    iic1.SetForwarding(0, true);
    iic1.SetDefaultRouteInAllNodes(0);

    Ipv6StaticRoutingHelper routingHelper;
    Ptr<Ipv6> ipv6Router = router.Get(0)->GetObject<Ipv6>();
    Ptr<Ipv6StaticRouting> routingRouter = routingHelper.GetStaticRouting(ipv6Router);

    routingRouter->AddNetworkRouteTo("2001:1::", Ipv6Prefix(64), 1);
    routingRouter->AddNetworkRouteTo("2001:2::", Ipv6Prefix(64), 2);

    Config::Set("/NodeList/*/DeviceList/*/$ns3::CsmaNetDevice/ReceiveErrorModel", PointerValue(nullptr));
    Config::Set("/NodeList/*/ApplicationList/*/$ns3::Ipv6L3Protocol/Rx", BooleanValue(true));

    RadvdHelper radvdHelper;
    radvdHelper.AddAnnouncedPrefix(router.Get(0), 1, Ipv6Address("2001:1::"), 64);
    radvdHelper.AddAnnouncedPrefix(router.Get(0), 2, Ipv6Address("2001:2::"), 64);
    radvdHelper.SetTimeBetweenRAs(Seconds(10));
    radvdHelper.SetMaxRtrAdvInterval(Seconds(100));
    radvdHelper.SetMinRtrAdvInterval(Seconds(5));
    radvdHelper.SetManagedFlag(true);
    radvdHelper.SetOtherConfigFlag(true);
    ApplicationContainer radvdApp = radvdHelper.Install(router);
    radvdApp.Start(Seconds(1.0));
    radvdApp.Stop(Seconds(10.0));

    V4PingHelper ping(nodes.Get(0)->GetObject<Ipv6>()->GetAddress(1, 0).GetAddress(0, 0).ToString());
    ping.SetRemote(nodes.Get(1)->GetObject<Ipv6>()->GetAddress(1, 0).GetAddress(0, 0));
    ping.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    ping.SetAttribute("Size", UintegerValue(1024));
    ApplicationContainer apps = ping.Install(nodes.Get(0));
    apps.Start(Seconds(2.0));
    apps.Stop(Seconds(10.0));

    AsciiTraceHelper ascii;
    Ptr<OutputStreamWrapper> stream = ascii.CreateFileStream("radvd.tr");
    csma.EnableAsciiAll(stream);

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}