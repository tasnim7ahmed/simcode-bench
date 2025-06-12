#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/internet-apps-module.h"
#include "ns3/csma-module.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/ipv6-routing-table.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/udp-echo-helper.h"
#include "ns3/netanim-module.h"
#include <iostream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("FragmentationIpv6");

int main(int argc, char *argv[]) {
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    NodeContainer nodes;
    nodes.Create(2);

    NodeContainer router;
    router.Create(1);

    CsmaHelper csma01;
    csma01.SetChannelAttribute("DataRate", DataRateValue(DataRate("5Mbps")));
    csma01.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));

    NetDeviceContainer devices0 = csma01.Install(NodeContainer(nodes.Get(0), router.Get(0)));
    NetDeviceContainer devices1 = csma01.Install(NodeContainer(nodes.Get(1), router.Get(0)));

    InternetStackHelper internetv6;
    internetv6.Install(nodes);
    internetv6.Install(router);

    Ipv6AddressHelper ipv6;
    ipv6.SetBase(Ipv6Address("2001:db8:0:1::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer interfaces0 = ipv6.Assign(devices0);
    ipv6.SetBase(Ipv6Address("2001:db8:0:2::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer interfaces1 = ipv6.Assign(devices1);

    // Set IPv6 global addresses on the interfaces
    interfaces0.SetForwarding(0, true);
    interfaces0.SetForwarding(1, true);
    interfaces1.SetForwarding(0, true);
    interfaces1.SetForwarding(1, true);

    Ipv6StaticRoutingHelper ipv6RoutingHelper;
    Ptr<Ipv6StaticRouting> staticRouting0 = ipv6RoutingHelper.GetStaticRouting(nodes.Get(0)->GetObject<Ipv6>()->GetRoutingProtocol());
    staticRouting0->AddGlobalRoute(Ipv6Address("2001:db8:0:2::/64"), interfaces0.GetAddress(1), 1);
    Ptr<Ipv6StaticRouting> staticRouting1 = ipv6RoutingHelper.GetStaticRouting(nodes.Get(1)->GetObject<Ipv6>()->GetRoutingProtocol());
    staticRouting1->AddGlobalRoute(Ipv6Address("2001:db8:0:1::/64"), interfaces1.GetAddress(1), 1);

    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(1));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient(interfaces1.GetAddress(0), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(1));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(2000););

    ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    csma01.EnablePcapAll("fragmentation-ipv6");

    Simulator::Stop(Seconds(11.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}