#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv6-routing-table-entry.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/radvd-interface.h"
#include "ns3/radvd-prefix.h"
#include "ns3/radvd.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("RadvdSimulation");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("Radvd", LOG_LEVEL_INFO);
    LogComponentEnable("Ping6Application", LOG_LEVEL_INFO);

    NodeContainer nodes;
    nodes.Create(2);

    NodeContainer router;
    router.Create(1);

    NodeContainer n0r = NodeContainer(nodes.Get(0), router.Get(0));
    NodeContainer n1r = NodeContainer(nodes.Get(1), router.Get(0));

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", DataRateValue(DataRate("100Mbps")));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));

    NetDeviceContainer devices_n0r = csma.Install(n0r);
    NetDeviceContainer devices_n1r = csma.Install(n1r);

    InternetStackHelper internetv6;
    internetv6.SetIpv4StackEnabled(false);
    internetv6.SetIpv6StackEnabled(true);
    internetv6.InstallAll();

    Ipv6AddressHelper ipv6;

    ipv6.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer iic_n0r = ipv6.Assign(devices_n0r);
    iic_n0r.SetForwarding(1, true);
    iic_n0r.SetDefaultRouteInfiniteGlobalUnicast(0);

    ipv6.SetBase(Ipv6Address("2001:2::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer iic_n1r = ipv6.Assign(devices_n1r);
    iic_n1r.SetForwarding(0, true);
    iic_n1r.SetDefaultRouteInfiniteGlobalUnicast(1);

    Ipv6StaticRoutingHelper routingHelper;
    Ptr<Ipv6StaticRouting> routingRouter = routingHelper.GetStaticRouting(router.Get(0)->GetObject<Ipv6>());
    routingRouter->AddNetworkRouteTo(Ipv6Address("2001:1::"), Ipv6Prefix(64), 1);
    routingRouter->AddNetworkRouteTo(Ipv6Address("2001:2::"), Ipv6Prefix(64), 2);

    RadvdHelper radvdHelper;

    radvdHelper.AddAnnouncedPrefix(router.Get(0), 1, Ipv6Address("2001:1::"), 64);
    radvdHelper.AddAnnouncedPrefix(router.Get(0), 2, Ipv6Address("2001:2::"), 64);

    radvdHelper.SetInterfaceMtu(router.Get(0), 1, 1500);
    radvdHelper.SetInterfaceMtu(router.Get(0), 2, 1500);

    radvdHelper.SetInterfaceInitialRtrAdvertisementInterval(router.Get(0), 1, Seconds(2));
    radvdHelper.SetInterfaceInitialRtrAdvertisementInterval(router.Get(0), 2, Seconds(2));
    radvdHelper.SetInterfaceMaxRtrAdvertisementInterval(router.Get(0), 1, Seconds(5));
    radvdHelper.SetInterfaceMaxRtrAdvertisementInterval(router.Get(0), 2, Seconds(5));
    radvdHelper.SetInterfaceMinRtrAdvertisementInterval(router.Get(0), 1, Seconds(3));
    radvdHelper.SetInterfaceMinRtrAdvertisementInterval(router.Get(0), 2, Seconds(3));
    radvdHelper.SetInterfaceMobRtrSupportFlag(router.Get(0), 1, false);
    radvdHelper.SetInterfaceMobRtrSupportFlag(router.Get(0), 2, false);

    ApplicationContainer radvdApp = radvdHelper.Install(router);
    radvdApp.Start(Seconds(1.0));
    radvdApp.Stop(Seconds(10.0));

    uint32_t packetSize = 1024;
    uint32_t maxPacketCount = 5;
    Time interPacketInterval = Seconds(1.0);

    Ping6Helper ping6(nodes.Get(0)->GetId(), nodes.Get(1)->GetObject<Ipv6>()->GetAddress(1, 0).GetAddress());
    ping6.SetAttribute("MaxPackets", UintegerValue(maxPacketCount));
    ping6.SetAttribute("Interval", TimeValue(interPacketInterval));
    ping6.SetAttribute("PacketSize", UintegerValue(packetSize));

    ApplicationContainer apps = ping6.Install(nodes.Get(0));
    apps.Start(Seconds(3.0));
    apps.Stop(Seconds(8.0));

    AsciiTraceHelper ascii;
    csma.EnableAsciiAll(ascii.CreateFileStream("radvd.tr"));
    csma.EnablePcapAll("radvd");

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}