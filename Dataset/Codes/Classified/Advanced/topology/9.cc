#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TREE_TOPOLOGY_EXAMPLE");

int main(int argc, char* argv[]) {
    // Parse command-line arguments
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    // Set time resolution and enable logging for client-server applications
    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create a node container with 4 nodes, representing a simple network
    NodeContainer tree;
    tree.Create(4);

    // Define point-to-point link characteristics (5 Mbps data rate, 2 ms delay)
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    // Create NetDevice containers to store the devices for each link
    NetDeviceContainer device1, device2, device3;

    // Install the point-to-point links between nodes (representing the tree structure)
    device1.Add(pointToPoint.Install(tree.Get(0), tree.Get(1))); // Link between node 0 and node 1
    device2.Add(pointToPoint.Install(tree.Get(0), tree.Get(2))); // Link between node 0 and node 2
    device3.Add(pointToPoint.Install(tree.Get(1), tree.Get(3))); // Link between node 1 and node 3

    // Install the internet stack on the nodes
    InternetStackHelper stack;
    stack.Install(tree);

    // Set up IP address helpers for each interface
    Ipv4AddressHelper address1, address2, address3;
    address1.SetBase("192.9.39.0", "255.255.255.252");
    address2.SetBase("192.9.39.4", "255.255.255.252");
    address3.SetBase("192.9.39.8", "255.255.255.252");

    // Create interface containers and assign IP addresses to each link
    Ipv4InterfaceContainer interface1, interface2, interface3;
    interface1 = address1.Assign(device1);
    interface2 = address2.Assign(device2);
    interface3 = address3.Assign(device3);

    // Setup UDP echo server on node 0
    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps01;
    serverApps01 = echoServer.Install(tree.Get(0)); // Install on node 0
    serverApps01.Start(Seconds(1.0));
    serverApps01.Stop(Seconds(10.0));

    // Setup UDP echo client on node 1 to communicate with server on node 0
    UdpEchoClientHelper echoClient01(interface1.GetAddress(0), 9); // Use address of node 0 as the server
    echoClient01.SetAttribute("MaxPackets", UintegerValue(1));
    echoClient01.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient01.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApp01 = echoClient01.Install(tree.Get(1)); // Install client on node 1
    clientApp01.Start(Seconds(2.0));
    clientApp01.Stop(Seconds(10.0));

    // Repeat the same for other client-server pairs

    // Server on node 1 and client on node 0
    ApplicationContainer serverApps10 = echoServer.Install(tree.Get(1)); // Server on node 1
    serverApps10.Start(Seconds(11.0));
    serverApps10.Stop(Seconds(20.0));

    UdpEchoClientHelper echoClient10(interface1.GetAddress(1), 9); // Use address of node 1 as the server
    echoClient10.SetAttribute("MaxPackets", UintegerValue(1));
    echoClient10.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient10.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApp10 = echoClient10.Install(tree.Get(0)); // Install client on node 0
    clientApp10.Start(Seconds(12.0));
    clientApp10.Stop(Seconds(20.0));

    // Server on node 0 and client on node 2
    ApplicationContainer serverApps02 = echoServer.Install(tree.Get(0)); // Server on node 0
    serverApps02.Start(Seconds(21.0));
    serverApps02.Stop(Seconds(30.0));

    UdpEchoClientHelper echoClient02(interface2.GetAddress(0), 9); // Use address of node 0 as the server
    echoClient02.SetAttribute("MaxPackets", UintegerValue(1));
    echoClient02.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient02.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApp02 = echoClient02.Install(tree.Get(2)); // Install client on node 2
    clientApp02.Start(Seconds(22.0));
    clientApp02.Stop(Seconds(30.0));

    // Server on node 2 and client on node 0
    ApplicationContainer serverApps20 = echoServer.Install(tree.Get(2)); // Server on node 2
    serverApps20.Start(Seconds(31.0));
    serverApps20.Stop(Seconds(40.0));

    UdpEchoClientHelper echoClient20(interface2.GetAddress(1), 9); // Use address of node 2 as the server
    echoClient20.SetAttribute("MaxPackets", UintegerValue(1));
    echoClient20.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient20.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApp20 = echoClient20.Install(tree.Get(0)); // Install client on node 0
    clientApp20.Start(Seconds(32.0));
    clientApp20.Stop(Seconds(40.0));

    // Server on node 1 and client on node 3
    ApplicationContainer serverApps13 = echoServer.Install(tree.Get(1)); // Server on node 1
    serverApps13.Start(Seconds(41.0));
    serverApps13.Stop(Seconds(50.0));

    UdpEchoClientHelper echoClient13(interface3.GetAddress(0), 9); // Use address of node 1 as the server
    echoClient13.SetAttribute("MaxPackets", UintegerValue(1));
    echoClient13.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient13.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApp13 = echoClient13.Install(tree.Get(3)); // Install client on node 3
    clientApp13.Start(Seconds(42.0));
    clientApp13.Stop(Seconds(50.0));

    // Server on node 3 and client on node 1
    ApplicationContainer serverApps31 = echoServer.Install(tree.Get(3)); // Server on node 3
    serverApps31.Start(Seconds(51.0));
    serverApps31.Stop(Seconds(60.0));

    UdpEchoClientHelper echoClient31(interface3.GetAddress(1), 9); // Use address of node 3 as the server
    echoClient31.SetAttribute("MaxPackets", UintegerValue(1));
    echoClient31.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient31.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApp31 = echoClient31.Install(tree.Get(1)); // Install client on node 1
    clientApp31.Start(Seconds(52.0));
    clientApp31.Stop(Seconds(60.0));

    // Enable animation output to view the topology
    AnimationInterface anim("Tree_Topology.xml");

    // Run the simulation and destroy after completion
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}

