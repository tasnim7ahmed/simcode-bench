#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv6-address.h"
#include "ns3/ipv6-routing-table-entry.h"
#include "ns3/internet-checksum.h"
#include "ns3/icmpv6-header.h"
#include "ns3/ipv6-extension-header.h"
#include "ns3/ipv6-hop-by-hop-extension-header.h"
#include "ns3/ipv6-destination-extension-header.h"
#include "ns3/ipv6-address-helper.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/node-container.h"
#include "ns3/net-device-container.h"
#include "ns3/ipv6-interface-container.h"
#include "ns3/command-line.h"
#include "ns3/simulator.h"
#include "ns3/uinteger.h"
#include "ns3/string.h"
#include "ns3/names.h"
#include "ns3/application-container.h"
#include "ns3/packet-sink-helper.h"
#include "ns3/icmpv6-echo.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    CommandLine cmd;
    cmd.Parse(argc, argv);

    NodeContainer nodes;
    nodes.Create(6);

    NodeContainer host0 = NodeContainer(nodes.Get(0));
    NodeContainer host1 = NodeContainer(nodes.Get(1));
    NodeContainer router0 = NodeContainer(nodes.Get(2));
    NodeContainer router1 = NodeContainer(nodes.Get(3));
    NodeContainer router2 = NodeContainer(nodes.Get(4));
    NodeContainer router3 = NodeContainer(nodes.Get(5));

    Names::Add("Host0", nodes.Get(0));
    Names::Add("Host1", nodes.Get(1));
    Names::Add("Router0", nodes.Get(2));
    Names::Add("Router1", nodes.Get(3));
    Names::Add("Router2", nodes.Get(4));
    Names::Add("Router3", nodes.Get(5));

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices02 = p2p.Install(NodeContainer(host0, router0));
    NetDeviceContainer devices03 = p2p.Install(NodeContainer(host0, router3));
    NetDeviceContainer devices12 = p2p.Install(NodeContainer(host1, router2));
    NetDeviceContainer devices13 = p2p.Install(NodeContainer(host1, router3));
    NetDeviceContainer devices20 = p2p.Install(NodeContainer(router2, router0));
    NetDeviceContainer devices21 = p2p.Install(NodeContainer(router2, router1));
    NetDeviceContainer devices30 = p2p.Install(NodeContainer(router3, router0));
    NetDeviceContainer devices31 = p2p.Install(NodeContainer(router3, router1));
    NetDeviceContainer devices01 = p2p.Install(NodeContainer(router0, router1));
    
    InternetStackHelper internetv6;
    internetv6.Install(nodes);

    Ipv6AddressHelper ipv6;
    ipv6.SetBase(Ipv6Address("2001:db8::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer interface02 = ipv6.Assign(devices02);
    Ipv6InterfaceContainer interface03 = ipv6.Assign(devices03);
    Ipv6InterfaceContainer interface12 = ipv6.Assign(devices12);
    Ipv6InterfaceContainer interface13 = ipv6.Assign(devices13);
    Ipv6InterfaceContainer interface20 = ipv6.Assign(devices20);
    Ipv6InterfaceContainer interface21 = ipv6.Assign(devices21);
    Ipv6InterfaceContainer interface30 = ipv6.Assign(devices30);
    Ipv6InterfaceContainer interface31 = ipv6.Assign(devices31);
    Ipv6InterfaceContainer interface01 = ipv6.Assign(devices01);

    interface02.GetAddress(0, 0).ScopeGlobal();
    interface03.GetAddress(0, 0).ScopeGlobal();
    interface12.GetAddress(0, 0).ScopeGlobal();
    interface13.GetAddress(0, 0).ScopeGlobal();
    interface20.GetAddress(0, 0).ScopeGlobal();
    interface21.GetAddress(0, 0).ScopeGlobal();
    interface30.GetAddress(0, 0).ScopeGlobal();
    interface31.GetAddress(0, 0).ScopeGlobal();
    interface01.GetAddress(0, 0).ScopeGlobal();

    for (uint32_t i = 0; i < devices02.GetN(); ++i) {
        nodes.Get(0)->GetObject<Ipv6>()->SetForwarding(0, true);
        nodes.Get(2)->GetObject<Ipv6>()->SetForwarding(0, true);
    }

    for (uint32_t i = 0; i < devices03.GetN(); ++i) {
        nodes.Get(0)->GetObject<Ipv6>()->SetForwarding(0, true);
        nodes.Get(5)->GetObject<Ipv6>()->SetForwarding(0, true);
    }
    
    for (uint32_t i = 0; i < devices12.GetN(); ++i) {
        nodes.Get(1)->GetObject<Ipv6>()->SetForwarding(0, true);
        nodes.Get(4)->GetObject<Ipv6>()->SetForwarding(0, true);
    }

    for (uint32_t i = 0; i < devices13.GetN(); ++i) {
        nodes.Get(1)->GetObject<Ipv6>()->SetForwarding(0, true);
        nodes.Get(5)->GetObject<Ipv6>()->SetForwarding(0, true);
    }

     for (uint32_t i = 0; i < devices20.GetN(); ++i) {
        nodes.Get(2)->GetObject<Ipv6>()->SetForwarding(0, true);
        nodes.Get(4)->GetObject<Ipv6>()->SetForwarding(0, true);
    }
    for (uint32_t i = 0; i < devices21.GetN(); ++i) {
        nodes.Get(2)->GetObject<Ipv6>()->SetForwarding(0, true);
        nodes.Get(3)->GetObject<Ipv6>()->SetForwarding(0, true);
    }
    for (uint32_t i = 0; i < devices30.GetN(); ++i) {
        nodes.Get(5)->GetObject<Ipv6>()->SetForwarding(0, true);
        nodes.Get(2)->GetObject<Ipv6>()->SetForwarding(0, true);
    }
     for (uint32_t i = 0; i < devices31.GetN(); ++i) {
        nodes.Get(5)->GetObject<Ipv6>()->SetForwarding(0, true);
        nodes.Get(3)->GetObject<Ipv6>()->SetForwarding(0, true);
    }
    for (uint32_t i = 0; i < devices01.GetN(); ++i) {
        nodes.Get(0)->GetObject<Ipv6>()->SetForwarding(0, true);
        nodes.Get(2)->GetObject<Ipv6>()->SetForwarding(0, true);
        nodes.Get(3)->GetObject<Ipv6>()->SetForwarding(0, true);
    }

    Ipv6Address destAddr = interface12.GetAddress(0, 0);

    Icmpv6EchoClientHelper echoClientHelper;
    echoClientHelper.SetAttribute("Interface", Ipv6InterfaceValue(interface02.Get(0)));

    ApplicationContainer pingApp = echoClientHelper.Install(host0.Get(0));
    pingApp.Start(Seconds(1.0));
    pingApp.Stop(Seconds(10.0));

    Ptr<Icmpv6Echo> pingAppPtr = DynamicCast<Icmpv6Echo>(pingApp.Get(0));
    pingAppPtr->SetRemote(destAddr);
    pingAppPtr->SetHopLimit(255);

    std::vector<Ipv6Address> route;
    route.push_back(interface02.GetAddress(1,0));
    route.push_back(interface21.GetAddress(1,0));
    pingAppPtr->SetLooseRouting(route);

    Simulator::Stop(Seconds(10.0));

    p2p.EnablePcapAll("loose-routing-ipv6");
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}