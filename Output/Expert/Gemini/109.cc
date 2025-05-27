#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/ipv6-routing-table-entry.h"
#include "ns3/icmpv6-echo.h"
#include "ns3/packet.h"
#include "ns3/ipv6-address.h"
#include "ns3/ipv6-interface-address.h"
#include "ns3/address.h"
#include "ns3/address-utils.h"
#include <iostream>

using namespace ns3;

int main(int argc, char *argv[]) {
    CommandLine cmd;
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("Icmpv6L4Protocol", LOG_LEVEL_INFO);
    LogComponentEnable("Ipv6L3Protocol", LOG_LEVEL_INFO);

    NodeContainer nodes;
    nodes.Create(6);

    NodeContainer host0 = NodeContainer(nodes.Get(0));
    NodeContainer host1 = NodeContainer(nodes.Get(1));
    NodeContainer router0 = NodeContainer(nodes.Get(2));
    NodeContainer router1 = NodeContainer(nodes.Get(3));
    NodeContainer router2 = NodeContainer(nodes.Get(4));
    NodeContainer router3 = NodeContainer(nodes.Get(5));

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices02 = p2p.Install(host0, router2);
    NetDeviceContainer devices21 = p2p.Install(router2, router1);
    NetDeviceContainer devices13 = p2p.Install(router1, router3);
    NetDeviceContainer devices31 = p2p.Install(router3, host1);

    InternetStackHelper internetv6;
    internetv6.Install(nodes);

    Ipv6AddressHelper ipv6;
    ipv6.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer interfaces02 = ipv6.Assign(devices02);

    ipv6.SetBase(Ipv6Address("2001:2::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer interfaces21 = ipv6.Assign(devices21);

    ipv6.SetBase(Ipv6Address("2001:3::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer interfaces13 = ipv6.Assign(devices13);

    ipv6.SetBase(Ipv6Address("2001:4::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer interfaces31 = ipv6.Assign(devices31);

    for (uint32_t i = 0; i < nodes.GetN(); ++i) {
        Ptr<Node> node = nodes.Get(i);
        Ptr<Ipv6L3Protocol> ipv6Proto = node->GetObject<Ipv6L3Protocol>();
        ipv6Proto->SetRoutingEnabled(true);
    }

    Ipv6StaticRoutingHelper ipv6RoutingHelper;

    Ptr<Ipv6StaticRouting> staticRoutingR2 = ipv6RoutingHelper.GetStaticRouting(router2->Get(0)->GetObject<Ipv6>());
    staticRoutingR2->AddHostRouteTo(Ipv6Address("2001:4::2"), 0, 1);
    staticRoutingR2->AddHostRouteTo(Ipv6Address("2001:1::1"), 0, 1);

    Ptr<Ipv6StaticRouting> staticRoutingR1 = ipv6RoutingHelper.GetStaticRouting(router1->Get(0)->GetObject<Ipv6>());
    staticRoutingR1->AddHostRouteTo(Ipv6Address("2001:1::1"), 0, 1);
    staticRoutingR1->AddHostRouteTo(Ipv6Address("2001:4::2"), 0, 1);

    Ptr<Ipv6StaticRouting> staticRoutingR3 = ipv6RoutingHelper.GetStaticRouting(router3->Get(0)->GetObject<Ipv6>());
    staticRoutingR3->AddHostRouteTo(Ipv6Address("2001:1::1"), 0, 1);
    staticRoutingR3->AddHostRouteTo(Ipv6Address("2001:4::2"), 0, 1);

    Packet::EnablePcapAll("loose-routing-ipv6", false);

    Ptr<Icmpv6Echo> echoClient = host0.Get(0)->GetObject<Icmpv6Echo>();
    Ipv6Address destAddr("2001:4::2");
    std::vector<Ipv6Address> routingAddresses;
    routingAddresses.push_back(interfaces21.GetAddress(0));
    routingAddresses.push_back(interfaces13.GetAddress(0));
    routingAddresses.push_back(interfaces31.GetAddress(0));
    echoClient->SendIcmpv6EchoRequest(destAddr, routingAddresses, 1, 1000);

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}