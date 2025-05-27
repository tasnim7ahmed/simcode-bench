#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/ipv6-routing-table-entry.h"
#include "ns3/ping6.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("RadvdTwoPrefix");

int main(int argc, char *argv[]) {
    CommandLine cmd;
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponent::SetDefaultLogLevel(LOG_LEVEL_INFO);
    LogComponent::SetLogLevel(
        "RadvdTwoPrefix", LOG_LEVEL_ALL); // Enable logging for this simulation

    // Create nodes
    NodeContainer nodes;
    nodes.Create(2);

    NodeContainer router;
    router.Create(1);

    // Create CSMA channels
    CsmaHelper csma01;
    csma01.SetChannelAttribute("DataRate", StringValue("10Mbps"));
    csma01.SetChannelAttribute("Delay", TimeValue(NanoSeconds(500)));

    CsmaHelper csma12;
    csma12.SetChannelAttribute("DataRate", StringValue("10Mbps"));
    csma12.SetChannelAttribute("Delay", TimeValue(NanoSeconds(500)));

    NetDeviceContainer nd0r = csma01.Install(NodeContainer(nodes.Get(0), router.Get(0)));
    NetDeviceContainer nd1r = csma12.Install(NodeContainer(nodes.Get(1), router.Get(0)));

    // Install Internet stack
    InternetStackHelper internet;
    internet.Install(nodes);
    internet.Install(router);

    // Assign IPv6 addresses
    Ipv6AddressHelper ipv6;
    ipv6.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer i0r = ipv6.Assign(nd0r);

    ipv6.SetBase(Ipv6Address("2001:2::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer i1r = ipv6.Assign(nd1r);

    // Enable IPv6 forwarding on the router
    Ipv6StaticRoutingHelper ipv6RoutingHelper;
    Ptr<Ipv6StaticRouting> routing = ipv6RoutingHelper.GetOrCreateRouting(router.Get(0)->GetObject<Ipv6> ());
    routing->SetDefaultRoute(i1r.GetAddress(0), 1);

    // Configure Router Advertisements
    Ptr<Radvd> radvd = CreateObject<Radvd>();
    radvd->SetAdvSendAdvert(true);

    // Interface 0 on Router
    radvd->AddInterface(router.Get(0)->GetDevice(0)->GetIfIndex());
    radvd->SetIface(router.Get(0)->GetDevice(0)->GetIfIndex(), "AdvSendAdvert", BooleanValue(true));

    //Set the first prefix for interface 0
    radvd->AddIfacePrefix(router->GetDevice(0)->GetIfIndex(), Ipv6Prefix("2001:1::/64"), true, true);
     //Set the second prefix for interface 0
    radvd->AddIfacePrefix(router->GetDevice(0)->GetIfIndex(), Ipv6Prefix("2001:ABCD::/64"), true, true);

    // Interface 1 on Router
    radvd->AddInterface(router.Get(0)->GetDevice(1)->GetIfIndex());
    radvd->SetIface(router.Get(0)->GetDevice(1)->GetIfIndex(), "AdvSendAdvert", BooleanValue(true));

    radvd->AddIfacePrefix(router->GetDevice(1)->GetIfIndex(), Ipv6Prefix("2001:2::/64"), true, true);

    router.Get(0)->AddApplication(radvd);
    radvd->SetStartTime(Seconds(1.0));
    radvd->SetStopTime(Seconds(10.0));

    // Print assigned addresses
    for (uint32_t i = 0; i < nodes.GetN(); ++i) {
        Ptr<Node> node = nodes.Get(i);
        Ptr<Ipv6> ipv6 = node->GetObject<Ipv6>();
        for (uint32_t j = 0; j < ipv6->GetNInterfaces(); ++j) {
            Ipv6InterfaceAddress iaddr = ipv6->GetAddress(j, 0);
            Ipv6Address addr = iaddr.GetAddress();
            NS_LOG_INFO("Node " << i << " Interface " << j << " Address: " << addr);
        }
    }

    Ptr<Node> node0 = nodes.Get(0);
    Ptr<Node> node1 = nodes.Get(1);

   // Create and install a ping application from node 0 to node 1
    Ping6Helper ping6;
    ping6.SetRemote(i1r.GetAddress(0));  //Ping from n0 to n1
    ping6.SetIfIndex(1);
    ApplicationContainer p = ping6.Install(node0);
    p.Start(Seconds(2.0));
    p.Stop(Seconds(10.0));

    // Tracing
    AsciiTraceHelper ascii;
    csma01.EnableAsciiAll(ascii.CreateFileStream("radvd-two-prefix.tr"));
    csma12.EnableAsciiAll(ascii.CreateFileStream("radvd-two-prefix.tr"));

    //Queue Tracing
    csma01.EnablePcapAll("radvd-two-prefix", false);
    csma12.EnablePcapAll("radvd-two-prefix", false);

    // Run the simulation
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}