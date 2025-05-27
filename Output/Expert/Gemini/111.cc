#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/ipv6-routing-table-entry.h"
#include "ns3/radvd-module.h"
#include "ns3/ping6.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    CommandLine cmd;
    cmd.Parse(argc, argv);

    NodeContainer nodes;
    nodes.Create(2);

    NodeContainer router;
    router.Create(1);

    CsmaHelper csma0;
    csma0.SetChannelAttribute("DataRate", DataRateValue(DataRate("10Mbps")));
    csma0.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));

    NetDeviceContainer devices0 = csma0.Install(NodeContainer(nodes.Get(0), router.Get(0)));

    CsmaHelper csma1;
    csma1.SetChannelAttribute("DataRate", DataRateValue(DataRate("10Mbps")));
    csma1.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));

    NetDeviceContainer devices1 = csma1.Install(NodeContainer(nodes.Get(1), router.Get(0)));

    InternetStackHelper internet;
    internet.Install(nodes);
    internet.Install(router);

    Ipv6AddressHelper ipv6;
    ipv6.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer interfaces0 = ipv6.Assign(devices0);

    ipv6.SetBase(Ipv6Address("2001:2::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer interfaces1 = ipv6.Assign(devices1);

    Ptr<Ipv6StaticRouting> staticRouting = Ipv6RoutingHelper::GetRouting(router.Get(0)->GetObject<Ipv6>());
    staticRouting->SetDefaultRoute(interfaces0.GetAddress(0,1),1);
    staticRouting = Ipv6RoutingHelper::GetRouting(router.Get(0)->GetObject<Ipv6>());
    staticRouting->SetDefaultRoute(interfaces1.GetAddress(0,1),1);

    RadvdHelper radvd0;
    radvd0.SetPrefix("2001:1::", 64);
    radvd0.SetAdvSendAdvert(true);
    radvd0.Install(devices0.Get(1));

    RadvdHelper radvd1;
    radvd1.SetPrefix("2001:2::", 64);
    radvd1.SetAdvSendAdvert(true);
    radvd1.Install(devices1.Get(1));

    Ipv6Address sinkAddr = interfaces1.GetAddress(0,0);

    Ping6Helper ping6;
    ping6.SetRemote(sinkAddr);
    ping6.SetIfIpv6(interfaces0.GetAddress(0,0));
    ping6.SetAttribute("Verbose", BooleanValue(true));

    ApplicationContainer apps = ping6.Install(nodes.Get(0));
    apps.Start(Seconds(1.0));
    apps.Stop(Seconds(10.0));

    Simulator::Stop(Seconds(10.0));

    CsmaHelper::EnablePcapAll("radvd", false);

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}