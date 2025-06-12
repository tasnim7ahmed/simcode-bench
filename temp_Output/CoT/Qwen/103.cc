#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv6-routing-table-entry.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("BasicIpv6Network");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("Icmpv6L4Protocol", LOG_LEVEL_INFO);
    LogComponentEnable("Ipv6Interface", LOG_LEVEL_ALL);

    NodeContainer nodes;
    nodes.Create(3);

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", DataRateValue(DataRate(5000000)));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));

    NetDeviceContainer devices1, devices2;
    devices1 = csma.Install(NodeContainer(nodes.Get(0), nodes.Get(1)));
    devices2 = csma.Install(NodeContainer(nodes.Get(1), nodes.Get(2)));

    InternetStackHelper stack;
    stack.InstallAll();

    Ipv6AddressHelper address;
    Ipv6InterfaceContainer interfaces1, interfaces2;

    address.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
    interfaces1 = address.Assign(devices1);
    interfaces1.SetForwarding(1, true);
    interfaces1.SetDefaultRouteInAllNodes(1);

    address.SetBase(Ipv6Address("2001:2::"), Ipv6Prefix(64));
    interfaces2 = address.Assign(devices2);
    interfaces2.SetForwarding(1, true);
    interfaces2.SetDefaultRouteInAllNodes(1);

    Ipv6StaticRoutingHelper routingHelper;
    Ptr<Ipv6> ipv6 = nodes.Get(1)->GetObject<Ipv6>();
    Ptr<Ipv6RoutingProtocol> routing = routingHelper.Create(ipv6);
    ipv6->SetRoutingProtocol(routing);

    Ipv6StaticRouting* routerRouting = dynamic_cast<Ipv6StaticRouting*>(routing.get());
    if (routerRouting) {
        routerRouting->AddNetworkRouteTo("2001:1::", Ipv6Prefix(64), 1);
        routerRouting->AddNetworkRouteTo("2001:2::", Ipv6Prefix(64), 2);
    }

    V4PingHelper ping(interfaces2.GetAddress(1, 1));
    ping.SetAttribute("Remote", Ipv6AddressValue(interfaces2.GetAddress(1)));
    ApplicationContainer apps = ping.Install(nodes.Get(0));
    apps.Start(Seconds(1.0));
    apps.Stop(Seconds(10.0));

    AsciiTraceHelper ascii;
    csma.EnableAsciiAll(ascii.CreateFileStream("basic-ipv6.tr"));
    csma.EnablePcapAll("basic-ipv6");

    Ipv6RoutingHelper::PrintRoutingTableAt(Seconds(2.0), nodes.Get(0), CreateFileStream("n0-rt.tbl"));

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}