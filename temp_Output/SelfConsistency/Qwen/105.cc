#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/ipv6-list-routing-helper.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("FragmentationIPv6Simulation");

int main(int argc, char *argv[]) {
    // Enable logging
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
    LogComponentEnable("Ipv6L3Protocol", LOG_LEVEL_INFO);
    LogComponentEnable("Ipv6Interface", LOG_LEVEL_INFO);

    // Create nodes
    NodeContainer nodes;
    nodes.Create(3);
    Ptr<Node> n0 = nodes.Get(0);
    Ptr<Node> r = nodes.Get(1);
    Ptr<Node> n1 = nodes.Get(2);

    // Create CSMA channels
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", DataRateValue(DataRate("100Mbps")));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));

    // Install Internet stack
    InternetStackHelper internetv6;
    Ipv6ListRoutingHelper listRH;

    Ipv6StaticRoutingHelper staticRh;
    listRH.Add(staticRh, 0);

    internetv6.SetRoutingHelper(listRH);
    internetv6.Install(nodes);

    // Create device containers and install devices
    NetDeviceContainer d_n0r, d_rn1;

    d_n0r = csma.Install(NodeContainer(n0, r));
    d_rn1 = csma.Install(NodeContainer(r, n1));

    // Assign IPv6 addresses
    Ipv6AddressHelper ipv6;

    ipv6.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer i_n0r = ipv6.Assign(d_n0r);
    i_n0r.SetForwarding(0, true);
    i_n0r.SetDefaultRouteInAllNodes(0);

    ipv6.SetBase(Ipv6Address("2001:2::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer i_rn1 = ipv6.Assign(d_rn1);
    i_rn1.SetForwarding(1, true);
    i_rn1.SetDefaultRouteInAllNodes(2);

    // Set up static routing
    Ptr<Ipv6StaticRouting> staticRoutingN0 = staticRh.GetStaticRouting(n0->GetObject<Ipv6>());
    staticRoutingN0->AddNetworkRouteTo(Ipv6Address("2001:2::"), Ipv6Prefix(64), 1);

    Ptr<Ipv6StaticRouting> staticRoutingR = staticRh.GetStaticRouting(r->GetObject<Ipv6>());
    staticRoutingR->AddNetworkRouteTo(Ipv6Address("2001:0::"), Ipv6Prefix(64), 0);
    staticRoutingR->AddNetworkRouteTo(Ipv6Address("2001:2::"), Ipv6Prefix(64), 2);

    Ptr<Ipv6StaticRouting> staticRoutingN1 = staticRh.GetStaticRouting(n1->GetObject<Ipv6>());
    staticRoutingN1->AddNetworkRouteTo(Ipv6Address("2001:1::"), Ipv6Prefix(64), 1);

    // Setup applications
    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(n1);
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient(i_rn1.GetAddress(1, 1), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(2000)); // Large enough to cause fragmentation

    ApplicationContainer clientApps = echoClient.Install(n0);
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Pcap tracing
    csma.EnablePcapAll("fragmentation-ipv6");

    // ASCII trace
    AsciiTraceHelper ascii;
    csma.EnableAsciiAll(ascii.CreateFileStream("fragmentation-ipv6.tr"));

    // Queue and packet reception tracing would be handled by the ASCII and pcap traces above

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}