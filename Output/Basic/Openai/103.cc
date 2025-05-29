#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);
    LogComponentEnable("V6Ping", LOG_LEVEL_INFO);

    NodeContainer nodes;
    nodes.Create(3); // n0, r, n1

    Ptr<Node> n0 = nodes.Get(0);
    Ptr<Node> r  = nodes.Get(1);
    Ptr<Node> n1 = nodes.Get(2);

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));

    // n0 - r
    NodeContainer net1(n0, r);
    NetDeviceContainer dev1 = csma.Install(net1);

    // r - n1
    NodeContainer net2(r, n1);
    NetDeviceContainer dev2 = csma.Install(net2);

    InternetStackHelper stack;
    stack.SetIpv6StackInstall(true);
    stack.Install(nodes);

    Ipv6AddressHelper ipv6a;
    ipv6a.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer iface1 = ipv6a.Assign(dev1);
    iface1.SetForwarding(1, true);
    iface1.SetDefaultRouteInAllNodes(1);

    ipv6a.SetBase(Ipv6Address("2001:2::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer iface2 = ipv6a.Assign(dev2);
    iface2.SetForwarding(0, true);
    iface2.SetDefaultRouteInAllNodes(0);

    // Manually set default routes
    Ptr<Ipv6StaticRouting> n0Static = Ipv6RoutingHelper::GetRouting<Ipv6StaticRouting>(n0->GetObject<Ipv6>()->GetRoutingProtocol());
    n0Static->SetDefaultRoute(iface1.GetAddress(1,1), 1);

    Ptr<Ipv6StaticRouting> n1Static = Ipv6RoutingHelper::GetRouting<Ipv6StaticRouting>(n1->GetObject<Ipv6>()->GetRoutingProtocol());
    n1Static->SetDefaultRoute(iface2.GetAddress(0,1), 1);

    // Install ICMPv6 Echo on n0, destination is n1's address
    uint32_t packetSize = 56;
    uint32_t maxPackets = 5;
    Time interPacketInterval = Seconds(1.0);

    V6PingHelper ping6(iface2.GetAddress(1,1));
    ping6.SetAttribute("PacketSize", UintegerValue(packetSize));
    ping6.SetAttribute("MaxPackets", UintegerValue(maxPackets));
    ping6.SetAttribute("Interval", TimeValue(interPacketInterval));
    ApplicationContainer apps = ping6.Install(n0);
    apps.Start(Seconds(2.0));
    apps.Stop(Seconds(10.0));

    // Enable tracing
    AsciiTraceHelper ascii;
    csma.EnableAsciiAll(ascii.CreateFileStream("csma-ipv6.tr"));
    csma.EnablePcapAll("csma-ipv6", true);

    // Print n0's IPv6 routing table
    Ipv6RoutingHelper::PrintRoutingTableAt(Seconds(1.5), n0);

    Simulator::Stop(Seconds(12.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}