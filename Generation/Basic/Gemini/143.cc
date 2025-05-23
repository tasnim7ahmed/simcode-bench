#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"

int main()
{
    // Enable logging for applications and IP protocol
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
    LogComponentEnable("Ipv4L3Protocol", LOG_LEVEL_INFO); // To observe IP packet forwarding

    // Create 4 nodes
    NodeContainer nodes;
    nodes.Create(4);

    Ptr<Node> n0 = nodes.Get(0); // Root Node
    Ptr<Node> n1 = nodes.Get(1); // Child Node 1
    Ptr<Node> n2 = nodes.Get(2); // Child Node 2
    Ptr<Node> n3 = nodes.Get(3); // Leaf Node

    // Create PointToPointHelper with specified attributes
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // Install devices for links to form the tree topology
    // Link 1: Root (N0) <-> Child 1 (N1)
    NetDeviceContainer devices01;
    devices01 = p2p.Install(n0, n1);

    // Link 2: Root (N0) <-> Child 2 (N2)
    NetDeviceContainer devices02;
    devices02 = p2p.Install(n0, n2);

    // Link 3: Child 1 (N1) <-> Leaf (N3)
    NetDeviceContainer devices13;
    devices13 = p2p.Install(n1, n3);

    // Install the Internet stack on all nodes
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses to the network interfaces
    Ipv4AddressHelper ipv4;

    // Assign IP addresses for Link 1 (N0-N1)
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer if01 = ipv4.Assign(devices01);

    // Assign IP addresses for Link 2 (N0-N2)
    ipv4.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer if02 = ipv4.Assign(devices02);

    // Assign IP addresses for Link 3 (N1-N3)
    ipv4.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer if13 = ipv4.Assign(devices13);

    // Populate routing tables using global routing (essential for multi-hop communication)
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Setup UDP Echo Server on the Root Node (N0)
    // The server listens on port 9
    UdpEchoServerHelper echoServer(9);

    ApplicationContainer serverApps = echoServer.Install(n0);
    serverApps.Start(Seconds(1.0)); // Server starts at 1 second
    serverApps.Stop(Seconds(10.0)); // Server stops at 10 seconds

    // Setup UDP Echo Client on the Leaf Node (N3)
    // The client sends packets to the Root Node's (N0) IP address on the N0-N1 link
    Ipv4Address rootIpAddress = if01.GetAddress(0); // N0's IP address is at index 0 in the devices01 container

    UdpEchoClientHelper echoClient(rootIpAddress, 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(1));   // Send 1 packet
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0))); // Wait 1 second between packets (if multiple)
    echoClient.SetAttribute("PacketSize", UintegerValue(1024)); // Packet size of 1024 bytes

    ApplicationContainer clientApps = echoClient.Install(n3);
    clientApps.Start(Seconds(2.0)); // Client starts at 2 seconds (after server is up)
    clientApps.Stop(Seconds(10.0)); // Client stops at 10 seconds

    // Set the simulation stop time
    Simulator::Stop(Seconds(11.0));

    // Run the simulation
    Simulator::Run();
    Simulator::Destroy(); // Clean up resources

    return 0;
}