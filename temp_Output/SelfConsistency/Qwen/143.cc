#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TreeTopologySimulation");

int main(int argc, char *argv[]) {
    // Enable logging
    LogComponentEnable("TreeTopologySimulation", LOG_LEVEL_INFO);

    // Create nodes for a tree topology: root (n0), two children (n1, n2), and leaf node (n3)
    NodeContainer nodes;
    nodes.Create(4);

    // Root node connects to child nodes
    PointToPointHelper p2pRootChild;
    p2pRootChild.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2pRootChild.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devRootToChild1 = p2pRootChild.Install(nodes.Get(0), nodes.Get(1));
    NetDeviceContainer devRootToChild2 = p2pRootChild.Install(nodes.Get(0), nodes.Get(2));

    // Child node 1 connects to leaf node
    NetDeviceContainer devChild1ToLeaf = p2pRootChild.Install(nodes.Get(1), nodes.Get(3));

    // Install Internet stack on all nodes
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;

    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer ifRootToChild1 = address.Assign(devRootToChild1);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer ifRootToChild2 = address.Assign(devRootToChild2);

    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer ifChild1ToLeaf = address.Assign(devChild1ToLeaf);

    // Set up routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Set up UDP echo client-server application from leaf node (n3) to root node (n0)
    uint16_t port = 9; // Echo server port

    // Server on root node
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // Client on leaf node
    UdpEchoClientHelper echoClient(ifRootToChild1.GetAddress(0), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = echoClient.Install(nodes.Get(3));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Enable PCAP tracing
    p2pRootChild.EnablePcapAll("tree-topology");

    // Optional: Animation
    AnimationInterface anim("tree-topology.xml");
    anim.SetConstantPosition(nodes.Get(0), 0, 0);
    anim.SetConstantPosition(nodes.Get(1), -50, 50);
    anim.SetConstantPosition(nodes.Get(2), 50, 50);
    anim.SetConstantPosition(nodes.Get(3), -100, 100);

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}