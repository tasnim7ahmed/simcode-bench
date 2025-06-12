#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/csma-module.h"
#include "ns3/ipv6-static-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("FragmentationIPv6Simulation");

int main(int argc, char *argv[]) {
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
    LogComponentEnable("Ipv6L3Protocol", LOG_LEVEL_INFO);
    LogComponentEnable("Icmpv6L4Protocol", LOG_LEVEL_INFO);

    NodeContainer nodes;
    nodes.Create(3);

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", DataRateValue(DataRate("5Mbps")));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));

    NetDeviceContainer devices0r = csma.Install(NodeContainer(nodes.Get(0), nodes.Get(2)));
    NetDeviceContainer devices1r = csma.Install(NodeContainer(nodes.Get(1), nodes.Get(2)));

    InternetStackHelper internet;
    internet.Install(nodes);

    Ipv6AddressHelper ipv6;
    ipv6.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer interfaces0r = ipv6.Assign(devices0r);
    interfaces0r.SetForwarding(1, true); // router interface

    ipv6.SetBase(Ipv6Address("2001:2::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer interfaces1r = ipv6.Assign(devices1r);
    interfaces1r.SetForwarding(0, true); // router interface

    Ipv6StaticRoutingHelper routingHelper;
    Ptr<Ipv6StaticRouting> routing0 = routingHelper.GetStaticRouting(nodes.Get(0)->GetObject<Ipv6>());
    routing0->AddRoute(Ipv6Address("2001:2::"), Ipv6Prefix(64), 1);

    Ptr<Ipv6StaticRouting> routing1 = routingHelper.GetStaticRouting(nodes.Get(1)->GetObject<Ipv6>());
    routing1->AddRoute(Ipv6Address("2001:1::"), Ipv6Prefix(64), 1);

    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(1));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient(interfaces1r.GetAddress(0, 1), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(2000)); // Large enough to require fragmentation

    ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    AsciiTraceHelper asciiHelper;
    Ptr<OutputStreamWrapper> stream = asciiHelper.CreateFileStream("fragmentation-ipv6.tr");
    csma.EnableAsciiAll(stream);

    csma.EnablePcapAll("fragmentation-ipv6");

    Simulator::Run();
    Simulator::Destroy();
    return 0;
}