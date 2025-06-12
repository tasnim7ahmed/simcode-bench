#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable logging for TCP applications
    LogComponentEnable("TcpClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("TcpServerApplication", LOG_LEVEL_INFO);

    // Create a ring topology with 4 nodes
    NodeContainer nodes;
    nodes.Create(4);

    // Connect nodes in a ring using point-to-point links
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("1ms"));

    NetDeviceContainer devices[4];

    // Connecting node 0 <-> 1
    devices[0] = p2p.Install(NodeContainer(nodes.Get(0), nodes.Get(1)));
    // Connecting node 1 <-> 2
    devices[1] = p2p.Install(NodeContainer(nodes.Get(1), nodes.Get(2)));
    // Connecting node 2 <-> 3
    devices[2] = p2p.Install(NodeContainer(nodes.Get(2), nodes.Get(3)));
    // Connecting node 3 <-> 0 to complete the ring
    devices[3] = p2p.Install(NodeContainer(nodes.Get(3), nodes.Get(0)));

    // Install Internet Stack on all nodes
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    Ipv4InterfaceContainer interfaces[4];

    address.SetBase("10.0.0.0", "255.255.255.0");
    interfaces[0] = address.Assign(devices[0]);
    address.NewNetwork();
    interfaces[1] = address.Assign(devices[1]);
    address.NewNetwork();
    interfaces[2] = address.Assign(devices[2]);
    address.NewNetwork();
    interfaces[3] = address.Assign(devices[3]);

    // Set up TCP Server on node 2
    uint16_t port = 5000;
    TcpServerHelper tcpServer(port);
    ApplicationContainer serverApp = tcpServer.Install(nodes.Get(2));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Set up TCP Client on node 0, connecting to node 2 via the ring
    TcpClientHelper tcpClient(interfaces[3].GetAddress(1), port); // node 3's interface to node 0
    tcpClient.SetAttribute("MaxPackets", UintegerValue(10));
    tcpClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    tcpClient.SetAttribute("PacketSize", UintegerValue(512));

    ApplicationContainer clientApp = tcpClient.Install(nodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Set up static routes so that traffic can traverse the ring
    Ipv4StaticRoutingHelper routingHelper;

    // Route from node 0 -> node 3 -> node 2 (destination)
    Ptr<Ipv4StaticRouting> route0 = routingHelper.GetStaticRouting(nodes.Get(0)->GetObject<Ipv4>());
    route0->AddHostRouteTo(interfaces[3].GetAddress(1), interfaces[3].GetAddress(0), 1); // to node 3

    Ptr<Ipv4StaticRouting> route3 = routingHelper.GetStaticRouting(nodes.Get(3)->GetObject<Ipv4>());
    route3->AddHostRouteTo(interfaces[2].GetAddress(1), interfaces[2].GetAddress(0), 1); // to node 2

    // Route from node 2 -> node 3 -> node 0 (for replies)
    Ptr<Ipv4StaticRouting> route2 = routingHelper.GetStaticRouting(nodes.Get(2)->GetObject<Ipv4>());
    route2->AddHostRouteTo(interfaces[1].GetAddress(0), interfaces[1].GetAddress(1), 1); // to node 1 (reverse path)

    // Enable NetAnim for visualization
    AnimationInterface anim("tcp-ring-topology.xml");

    // Add descriptions to nodes
    anim.SetConstantPosition(nodes.Get(0), 0.0, 0.0);
    anim.SetConstantPosition(nodes.Get(1), 5.0, 5.0);
    anim.SetConstantPosition(nodes.Get(2), 10.0, 0.0);
    anim.SetConstantPosition(nodes.Get(3), 5.0, -5.0);

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}