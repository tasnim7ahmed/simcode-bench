#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable logging
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create nodes
    NodeContainer nodes;
    nodes.Create(5);
    NodeContainer srcNode = NodeContainer(nodes.Get(0));
    NodeContainer n0Node = NodeContainer(nodes.Get(1));
    NodeContainer rNode = NodeContainer(nodes.Get(2));
    NodeContainer n1Node = NodeContainer(nodes.Get(3));
    NodeContainer dstNode = NodeContainer(nodes.Get(4));

    // Create channels
    CsmaHelper csma01, csma12;
    csma01.SetChannelAttribute("DataRate", StringValue("5Mbps"));
    csma01.SetChannelAttribute("Delay", StringValue("2ms"));
    csma01.SetDeviceAttribute("Mtu", UintegerValue(5000));

    csma12.SetChannelAttribute("DataRate", StringValue("5Mbps"));
    csma12.SetChannelAttribute("Delay", StringValue("2ms"));
    csma12.SetDeviceAttribute("Mtu", UintegerValue(1500));

    NetDeviceContainer devices01 = csma01.Install(NodeContainer(srcNode, n0Node));
    NetDeviceContainer devices1r = csma01.Install(NodeContainer(n0Node, rNode));
    NetDeviceContainer devicesr2 = csma12.Install(NodeContainer(rNode, n1Node));
    NetDeviceContainer devices2d = csma01.Install(NodeContainer(n1Node, dstNode));

    // Install IPv6 stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IPv6 Addresses
    Ipv6AddressHelper address;
    address.SetBase(Ipv6Address("2001:db8:1::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer interfaces01 = address.Assign(devices01);
    interfaces01.SetForwarding(1, true);
    interfaces01.SetDefaultRouteInAllNodes(1);

    Ipv6InterfaceContainer interfaces1r = address.Assign(devices1r);
    interfaces1r.SetForwarding(1, true);
    interfaces1r.SetDefaultRouteInAllNodes(1);

    address.SetBase(Ipv6Address("2001:db8:2::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer interfacesr2 = address.Assign(devicesr2);
    interfacesr2.SetForwarding(0, true);
    interfacesr2.SetForwarding(1, true);

    Ipv6InterfaceContainer interfaces2d = address.Assign(devices2d);
    interfaces2d.SetForwarding(1, true);

    Ipv6GlobalRoutingHelper::PopulateRoutingTables();

    // Setup UDP echo server on node Dst
    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(dstNode.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // Setup UDP echo client on node Src sending to node Dst
    UdpEchoClientHelper echoClient(interfaces2d.GetAddress(1), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(4000));

    ApplicationContainer clientApps = echoClient.Install(srcNode.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Set up tracing
    AsciiTraceHelper ascii;
    csma01.EnableAsciiAll(ascii.CreateFileStream("fragmentation-ipv6-two-mtu.tr"));

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}