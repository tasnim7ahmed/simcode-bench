#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable logging for debugging
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create three nodes
    NodeContainer nodes;
    nodes.Create(3);

    // Configure point-to-point links
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // First link between node 0 and node 1
    NetDeviceContainer devices01 = p2p.Install(nodes.Get(0), nodes.Get(1));

    // Second link between node 1 and node 2
    NetDeviceContainer devices12 = p2p.Install(nodes.Get(1), nodes.Get(2));

    // Install Internet stack on all nodes
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses to interfaces
    Ipv4AddressHelper address01;
    address01.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces01 = address01.Assign(devices01);

    Ipv4AddressHelper address12;
    address12.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces12 = address12.Assign(devices12);

    // Manually configure routing so that node 0 knows how to reach node 2 via node 1
    Ipv4StaticRoutingHelper routingHelper;

    // Add route from node 0: network 10.1.2.0/24 is reachable via next hop 10.1.1.2 (node 1)
    Ptr<Ipv4StaticRouting> routingNode0 = routingHelper.GetStaticRouting(nodes.Get(0)->GetObject<Ipv4>());
    routingNode0->AddNetworkRouteTo(Ipv4Address("10.1.2.0"), Ipv4Mask("255.255.255.0"), 1);

    // Add route from node 2: network 10.1.1.0/24 is reachable via next hop 10.1.2.1 (node 1)
    Ptr<Ipv4StaticRouting> routingNode2 = routingHelper.GetStaticRouting(nodes.Get(2)->GetObject<Ipv4>());
    routingNode2->AddNetworkRouteTo(Ipv4Address("10.1.1.0"), Ipv4Mask("255.255.255.0"), 1);

    // Set up the UDP Echo Server on node 2 (third node)
    uint16_t port = 9;
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApp = echoServer.Install(nodes.Get(2));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Set up the UDP Echo Client on node 0 (first node)
    UdpEchoClientHelper echoClient(interfaces12.GetAddress(1), port); // destination is node 2's IP
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp = echoClient.Install(nodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Run the simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}