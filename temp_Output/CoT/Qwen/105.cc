#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/csma-module.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/ipv6-list-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("FragmentationIPv6Simulation");

int main(int argc, char *argv[]) {
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
    LogComponentEnable("Ipv6Interface", LOG_LEVEL_ALL);

    NodeContainer nodes;
    nodes.Create(2);

    NodeContainer router;
    router.Create(1);

    NodeContainer p2pNodes0 = NodeContainer(nodes.Get(0), router.Get(0));
    NodeContainer p2pNodes1 = NodeContainer(router.Get(0), nodes.Get(1));

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", DataRateValue(DataRate("100Mbps")));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));

    NetDeviceContainer devices0 = csma.Install(p2pNodes0);
    NetDeviceContainer devices1 = csma.Install(p2pNodes1);

    InternetStackHelper internetv6;
    Ipv6ListRoutingHelper listRH;
    Ipv6StaticRoutingHelper staticRh;

    listRH.Add(staticRh, 0);

    internetv6.SetRoutingHelper(listRH);
    internetv6.InstallAll();

    Ipv6AddressHelper address;
    address.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer interfaces0 = address.Assign(devices0);
    interfaces0.SetForwarding(0, true);
    interfaces0.SetDefaultRouteInOneDirection(0);

    address.SetBase(Ipv6Address("2001:2::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer interfaces1 = address.Assign(devices1);
    interfaces1.SetForwarding(0, true);
    interfaces1.SetDefaultRouteInOneDirection(0);

    // Add default route from n0 to router
    Ptr<Ipv6> ipv6_n0 = nodes.Get(0)->GetObject<Ipv6>();
    Ptr<Ipv6RoutingProtocol> routing_n0 = ipv6_n0->GetRoutingProtocol();
    Ptr<Ipv6StaticRouting> staticRouting_n0 = DynamicCast<Ipv6StaticRouting>(routing_n0);
    staticRouting_n0->AddNetworkRouteTo(Ipv6Address("2001:2::"), Ipv6Prefix(64), 1);

    // Add default route from n1 to router
    Ptr<Ipv6> ipv6_n1 = nodes.Get(1)->GetObject<Ipv6>();
    Ptr<Ipv6RoutingProtocol> routing_n1 = ipv6_n1->GetRoutingProtocol();
    Ptr<Ipv6StaticRouting> staticRouting_n1 = DynamicCast<Ipv6StaticRouting>(routing_n1);
    staticRouting_n1->AddNetworkRouteTo(Ipv6Address("2001:1::"), Ipv6Prefix(64), 1);

    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(1));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient(interfaces1.GetAddress(0, 1), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(2000)); // Large enough to cause fragmentation

    ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    AsciiTraceHelper asciiTraceHelper;
    Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream("fragmentation-ipv6.tr");
    csma.EnableAsciiAll(stream);
    csma.EnablePcapAll("fragmentation-ipv6"); 

    Simulator::Run();
    Simulator::Destroy();
    return 0;
}