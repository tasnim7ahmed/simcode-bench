#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv6-routing-table-entry.h"
#include "ns3/radvd.h"
#include "ns3/radvd-interface.h"
#include "ns3/radvd-prefix.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("RadvdSimulation");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("Radvd", LOG_LEVEL_INFO);

    NodeContainer nodes;
    nodes.Create(2);

    NodeContainer router;
    router.Create(1);

    NodeContainer net0(nodes.Get(0), router.Get(0));
    NodeContainer net1(nodes.Get(1), router.Get(0));

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", DataRateValue(DataRate("100Mbps")));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));

    NetDeviceContainer ndc0 = csma.Install(net0);
    NetDeviceContainer ndc1 = csma.Install(net1);

    InternetStackHelper internetv6;
    internetv6.SetIpv4StackEnabled(false);
    internetv6.SetIpv6StackEnabled(true);
    internetv6.InstallAll();

    Ipv6AddressHelper ipv6;

    ipv6.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer iic0 = ipv6.Assign(ndc0);
    iic0.SetForwarding(1, true);
    iic0.SetDefaultRouteInAllNodes(1);

    ipv6.SetBase(Ipv6Address("2001:2::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer iic1 = ipv6.Assign(ndc1);
    iic1.SetForwarding(1, true);
    iic1.SetDefaultRouteInAllNodes(1);

    Ipv6InterfaceContainer routerInterfaces;
    routerInterfaces.Add(iic0.Get(1));
    routerInterfaces.Add(iic1.Get(1));

    RadvdHelper radvdHelper;

    // Configure prefix for subnet 2001:1::/64 on interface to n0
    radvdHelper.AddAnnouncedPrefix(router.Get(0), 0, Ipv6Address("2001:1::"), 64);
    radvdHelper.SetRouterAdvertInterval(router.Get(0), 0, Seconds(10));
    radvdHelper.SetManagedFlag(router.Get(0), 0, true);
    radvdHelper.SetOtherConfigFlag(router.Get(0), 0, true);

    // Configure prefix for subnet 2001:2::/64 on interface to n1
    radvdHelper.AddAnnouncedPrefix(router.Get(0), 1, Ipv6Address("2001:2::"), 64);
    radvdHelper.SetRouterAdvertInterval(router.Get(0), 1, Seconds(10));
    radvdHelper.SetManagedFlag(router.Get(0), 1, true);
    radvdHelper.SetOtherConfigFlag(router.Get(0), 1, true);

    ApplicationContainer radvdApp = radvdHelper.Install(router);
    radvdApp.Start(Seconds(1.0));
    radvdApp.Stop(Seconds(10.0));

    V4PingHelper ping6(nodes.Get(1)->GetObject<Ipv6>()->GetAddress(1, 0).GetAddress(1));
    ping6.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    ping6.SetAttribute("Size", UintegerValue(1024));
    ApplicationContainer apps = ping6.Install(nodes.Get(0));
    apps.Start(Seconds(2.0));
    apps.Stop(Seconds(10.0));

    AsciiTraceHelper ascii;
    Ptr<OutputStreamWrapper> stream = ascii.CreateFileStream("radvd.tr");
    csma.EnableAsciiAll(stream);

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}