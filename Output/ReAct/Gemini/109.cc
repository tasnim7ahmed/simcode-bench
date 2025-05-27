#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/ipv6-routing-table-entry.h"
#include "ns3/icmpv6-echo.h"
#include "ns3/ipv6-extension-header.h"
#include "ns3/flow-classifier.h"
#include "ns3/flow-monitor-helper.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    CommandLine cmd;
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);

    NodeContainer hosts;
    hosts.Create(2);

    NodeContainer routers;
    routers.Create(4);

    NodeContainer allNodes;
    allNodes.Add(hosts);
    allNodes.Add(routers);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("1ms"));

    NetDeviceContainer host0Router0Devs = p2p.Install(hosts.Get(0), routers.Get(0));
    NetDeviceContainer host1Router3Devs = p2p.Install(hosts.Get(1), routers.Get(3));
    NetDeviceContainer router0Router1Devs = p2p.Install(routers.Get(0), routers.Get(1));
    NetDeviceContainer router0Router2Devs = p2p.Install(routers.Get(0), routers.Get(2));
    NetDeviceContainer router1Router3Devs = p2p.Install(routers.Get(1), routers.Get(3));
    NetDeviceContainer router2Router3Devs = p2p.Install(routers.Get(2), routers.Get(3));

    InternetStackHelper internetv6;
    internetv6.Install(allNodes);

    Ipv6AddressHelper ipv6;
    ipv6.SetBase(Ipv6Address("2001:db8:0:1::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer host0Router0Ifaces = ipv6.Assign(host0Router0Devs);

    ipv6.SetBase(Ipv6Address("2001:db8:0:2::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer host1Router3Ifaces = ipv6.Assign(host1Router3Devs);

    ipv6.SetBase(Ipv6Address("2001:db8:0:3::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer router0Router1Ifaces = ipv6.Assign(router0Router1Devs);

    ipv6.SetBase(Ipv6Address("2001:db8:0:4::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer router0Router2Ifaces = ipv6.Assign(router0Router2Devs);

    ipv6.SetBase(Ipv6Address("2001:db8:0:5::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer router1Router3Ifaces = ipv6.Assign(router1Router3Devs);

    ipv6.SetBase(Ipv6Address("2001:db8:0:6::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer router2Router3Ifaces = ipv6.Assign(router2Router3Devs);

    Ipv6StaticRoutingHelper ipv6RoutingHelper;

    Ptr<Ipv6StaticRouting> staticRoutingRouter0 = ipv6RoutingHelper.GetStaticRouting(routers.Get(0)->GetObject<Ipv6>());
    staticRoutingRouter0->AddHostRouteToNet(Ipv6Address("2001:db8:0:2::1"), 
                                           Ipv6Address("2001:db8:0:3::2"), 1);
    staticRoutingRouter0->AddNetworkRouteToNet(Ipv6Address("2001:db8:0:2::"), 
                                              Ipv6Prefix(64), 
                                              Ipv6Address("2001:db8:0:3::2"), 1);
    staticRoutingRouter0->AddNetworkRouteToNet(Ipv6Address("2001:db8:0:5::"),
                                              Ipv6Prefix(64),
                                              Ipv6Address("2001:db8:0:3::2"), 1);
    staticRoutingRouter0->AddNetworkRouteToNet(Ipv6Address("2001:db8:0:6::"),
                                              Ipv6Prefix(64),
                                              Ipv6Address("2001:db8:0:4::2"), 1);

    Ptr<Ipv6StaticRouting> staticRoutingRouter1 = ipv6RoutingHelper.GetStaticRouting(routers.Get(1)->GetObject<Ipv6>());
    staticRoutingRouter1->AddHostRouteToNet(Ipv6Address("2001:db8:0:2::1"), 
                                           Ipv6Address("2001:db8:0:5::2"), 1);
    staticRoutingRouter1->AddNetworkRouteToNet(Ipv6Address("2001:db8:0:2::"), 
                                              Ipv6Prefix(64), 
                                              Ipv6Address("2001:db8:0:5::2"), 1);

    Ptr<Ipv6StaticRouting> staticRoutingRouter2 = ipv6RoutingHelper.GetStaticRouting(routers.Get(2)->GetObject<Ipv6>());
     staticRoutingRouter2->AddHostRouteToNet(Ipv6Address("2001:db8:0:2::1"),
                                              Ipv6Address("2001:db8:0:6::2"), 1);
     staticRoutingRouter2->AddNetworkRouteToNet(Ipv6Address("2001:db8:0:2::"),
                                                 Ipv6Prefix(64),
                                                 Ipv6Address("2001:db8:0:6::2"), 1);
    Ptr<Ipv6StaticRouting> staticRoutingRouter3 = ipv6RoutingHelper.GetStaticRouting(routers.Get(3)->GetObject<Ipv6>());
    staticRoutingRouter3->AddHostRouteToNet(Ipv6Address("2001:db8:0:1::1"), 
                                           Ipv6Address("2001:db8:0:5::1"), 1);
    staticRoutingRouter3->AddHostRouteToNet(Ipv6Address("2001:db8:0:1::1"), 
                                           Ipv6Address("2001:db8:0:6::1"), 1);
    staticRoutingRouter3->AddNetworkRouteToNet(Ipv6Address("2001:db8:0:1::"), 
                                              Ipv6Prefix(64), 
                                              Ipv6Address("2001:db8:0:5::1"), 1);
    staticRoutingRouter3->AddNetworkRouteToNet(Ipv6Address("2001:db8:0:3::"),
                                              Ipv6Prefix(64),
                                              Ipv6Address("2001:db8:0:5::1"), 1);
    staticRoutingRouter3->AddNetworkRouteToNet(Ipv6Address("2001:db8:0:4::"),
                                              Ipv6Prefix(64),
                                              Ipv6Address("2001:db8:0:6::1"), 1);
    
    
    Icmpv6EchoClientHelper echoClientHelper;
    echoClientHelper.SetAttribute("MaxBytes", UintegerValue(1024));
    echoClientHelper.SetAttribute("Interval", TimeValue(Seconds(1)));
    echoClientHelper.SetAttribute("Size", UintegerValue(512));
    
    ApplicationContainer echoClientApp = echoClientHelper.Install(hosts.Get(0));
    echoClientApp.Start(Seconds(2.0));
    echoClientApp.Stop(Seconds(10.0));

    Ptr<Icmpv6Echo> echoClient = DynamicCast<Icmpv6Echo>(echoClientApp.Get(0));
    
    Ipv6Address nextHop1 = Ipv6Address("2001:db8:0:3::2");
    Ipv6Address nextHop2 = Ipv6Address("2001:db8:0:6::2");

    std::vector<Ipv6Address> destList;
    destList.push_back(nextHop1);
    destList.push_back(nextHop2);
    destList.push_back(Ipv6Address("2001:db8:0:2::1"));
    
    echoClient->SetLooseSource(destList);

    echoClient->SetRemote(Ipv6Address("2001:db8:0:2::1"));
    
    p2p.EnablePcapAll("loose-routing-ipv6");

    Simulator::Stop(Seconds(11.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}