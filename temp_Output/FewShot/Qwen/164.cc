#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-routing-helper.h"
#include "ns3/ipv4-ospf-routing-helper.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable OSPF logging for debugging (optional)
    LogComponentEnable("Ipv4OspfRouting", LOG_LEVEL_INFO);

    // Create a network topology with 4 nodes
    NodeContainer nodes;
    nodes.Create(4);

    // Connect nodes in a mesh-like topology using point-to-point links
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // Install internet stack on all nodes
    InternetStackHelper stack;
    Ipv4OspfRoutingHelper ospfRouting;
    stack.SetRoutingHelper(ospfRouting); // Assign OSPF routing
    stack.Install(nodes);

    // Create links between:
    // Node 0 <-> Node 1
    // Node 1 <-> Node 2
    // Node 1 <-> Node 3
    // Node 2 <-> Node 3

    NetDeviceContainer devices01 = p2p.Install(NodeContainer(nodes.Get(0), nodes.Get(1)));
    NetDeviceContainer devices12 = p2p.Install(NodeContainer(nodes.Get(1), nodes.Get(2)));
    NetDeviceContainer devices13 = p2p.Install(NodeContainer(nodes.Get(1), nodes.Get(3)));
    NetDeviceContainer devices23 = p2p.Install(NodeContainer(nodes.Get(2), nodes.Get(3)));

    // Assign IP addresses to interfaces
    Ipv4AddressHelper address;

    address.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces01 = address.Assign(devices01);
    address.NewNetwork();
    Ipv4InterfaceContainer interfaces12 = address.Assign(devices12);
    address.NewNetwork();
    Ipv4InterfaceContainer interfaces13 = address.Assign(devices13);
    address.NewNetwork();
    Ipv4InterfaceContainer interfaces23 = address.Assign(devices23);

    // Set up a simple UDP echo application from Node 0 to Node 3
    uint16_t port = 9;
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApp = echoServer.Install(nodes.Get(3));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient(interfaces13.GetAddress(1), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp = echoClient.Install(nodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Enable NetAnim visualization
    AnimationInterface anim("ospf-routing-animation.xml");

    // Optionally assign node positions for better visualization (NetAnim uses these if defined)
    anim.SetMobilityPollInterval(Seconds(0.5));

    // Run the simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}