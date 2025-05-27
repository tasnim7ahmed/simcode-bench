#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/ipv6-routing-table-entry.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    NodeContainer srcNode, n0Node, r1Node, n1Node, r2Node, n2Node;
    srcNode.Create(1);
    n0Node.Create(1);
    r1Node.Create(1);
    n1Node.Create(1);
    r2Node.Create(1);
    n2Node.Create(1);

    NodeContainer net0Nodes = NodeContainer(srcNode, n0Node, r1Node);
    NodeContainer net1Nodes = NodeContainer(r1Node, n1Node, r2Node);
    NodeContainer net2Nodes = NodeContainer(r2Node, n2Node);

    CsmaHelper csma0, csma1;
    PointToPointHelper p2p2;

    csma0.SetChannelAttribute("DataRate", StringValue("5Mbps"));
    csma0.SetChannelAttribute("Delay", StringValue("2ms"));
    csma0.SetDeviceAttribute("Mtu", UintegerValue(5000));

    csma1.SetChannelAttribute("DataRate", StringValue("5Mbps"));
    csma1.SetChannelAttribute("Delay", StringValue("2ms"));
    csma1.SetDeviceAttribute("Mtu", UintegerValue(2000));

    p2p2.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p2.SetChannelAttribute("Delay", StringValue("2ms"));
    p2p2.SetDeviceAttribute("Mtu", UintegerValue(1500));

    NetDeviceContainer net0Devices = csma0.Install(net0Nodes);
    NetDeviceContainer net1Devices = csma1.Install(net1Nodes);
    NetDeviceContainer net2Devices = p2p2.Install(net2Nodes);

    InternetStackHelper internetv6;
    internetv6.Install(srcNode);
    internetv6.Install(n0Node);
    internetv6.Install(r1Node);
    internetv6.Install(n1Node);
    internetv6.Install(r2Node);
    internetv6.Install(n2Node);

    Ipv6AddressHelper ipv6;
    ipv6.SetBase(Ipv6Address("2001:db8:1::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer net0Interfaces = ipv6.Assign(net0Devices);
    ipv6.SetBase(Ipv6Address("2001:db8:2::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer net1Interfaces = ipv6.Assign(net1Devices);
    ipv6.SetBase(Ipv6Address("2001:db8:3::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer net2Interfaces = ipv6.Assign(net2Devices);

    Ipv6StaticRoutingHelper ipv6RoutingHelper;
    Ptr<Ipv6StaticRouting> staticRouting;

    staticRouting = ipv6RoutingHelper.GetOrCreateRouting(srcNode.Get(0)->GetObject<Ipv6>()->GetRoutingProtocol());
    staticRouting->SetDefaultRoute(net0Interfaces.GetAddress(2,1), 1);

    staticRouting = ipv6RoutingHelper.GetOrCreateRouting(n0Node.Get(0)->GetObject<Ipv6>()->GetRoutingProtocol());
    staticRouting->SetDefaultRoute(net0Interfaces.GetAddress(2,1), 1);

    staticRouting = ipv6RoutingHelper.GetOrCreateRouting(r1Node.Get(0)->GetObject<Ipv6>()->GetRoutingProtocol());
    staticRouting->AddHostRouteTo(Ipv6Address("2001:db8:1::1"), net0Interfaces.GetAddress(0,1), 1);
    staticRouting->AddHostRouteTo(Ipv6Address("2001:db8:1::2"), net0Interfaces.GetAddress(1,1), 1);
    staticRouting->AddHostRouteTo(Ipv6Address("2001:db8:3::2"), net1Interfaces.GetAddress(2,1), 1);

    staticRouting = ipv6RoutingHelper.GetOrCreateRouting(n1Node.Get(0)->GetObject<Ipv6>()->GetRoutingProtocol());
    staticRouting->SetDefaultRoute(net1Interfaces.GetAddress(0,1), 1);

    staticRouting = ipv6RoutingHelper.GetOrCreateRouting(r2Node.Get(0)->GetObject<Ipv6>()->GetRoutingProtocol());
    staticRouting->AddHostRouteTo(Ipv6Address("2001:db8:1::1"), net1Interfaces.GetAddress(0,1), 1);
    staticRouting->AddHostRouteTo(Ipv6Address("2001:db8:1::2"), net1Interfaces.GetAddress(0,1), 1);
    staticRouting->SetDefaultRoute(net2Interfaces.GetAddress(1,1), 1);

    staticRouting = ipv6RoutingHelper.GetOrCreateRouting(n2Node.Get(0)->GetObject<Ipv6>()->GetRoutingProtocol());
    staticRouting->SetDefaultRoute(net2Interfaces.GetAddress(0,1), 1);

    uint16_t port = 9;
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApp = echoServer.Install(n2Node.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient(net2Interfaces.GetAddress(1), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(1));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(4000));
    ApplicationContainer clientApp = echoClient.Install(srcNode.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    Simulator::Stop(Seconds(10.0));
    
    AsciiTraceHelper ascii;
    Ptr<OutputStreamWrapper> stream = ascii.CreateFileStream("fragmentation-ipv6-PMTU.tr");
    csma0.EnableAsciiAll(stream);
    csma1.EnableAsciiAll(stream);
    p2p2.EnableAsciiAll(stream);
    
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}