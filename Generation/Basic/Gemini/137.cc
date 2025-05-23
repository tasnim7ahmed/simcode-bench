#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

// Define ns-3 namespace to avoid prefixing
using namespace ns3;

int
main(int argc, char *argv[])
{
    // Enable logging for relevant components
    // UdpEchoClient/Server will log packet transmission/reception
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
    // PointToPointNetDevice will log device-level events, including packet Tx/Rx at device level
    LogComponentEnable("PointToPointNetDevice", LOG_LEVEL_INFO);
    // Ipv4GlobalRouting will log routing table population
    LogComponentEnable("Ipv4GlobalRouting", LOG_LEVEL_INFO);

    // Allow users to override default attributes
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    // Create three nodes: Node 0 (Client), Node 1 (Router), Node 2 (Server)
    NodeContainer nodes;
    nodes.Create(3);

    // Create PointToPointHelper and set link attributes
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // Install net devices for the first link (Node 0 - Node 1)
    NetDeviceContainer devices01;
    devices01 = p2p.Install(nodes.Get(0), nodes.Get(1));

    // Install net devices for the second link (Node 1 - Node 2)
    NetDeviceContainer devices12;
    devices12 = p2p.Install(nodes.Get(1), nodes.Get(2));

    // Install the Internet stack on all nodes
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses to the first link (10.1.1.0/24)
    Ipv4AddressHelper address01;
    address01.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces01;
    interfaces01 = address01.Assign(devices01); // Node 0: 10.1.1.1, Node 1: 10.1.1.2

    // Assign IP addresses to the second link (10.1.2.0/24)
    Ipv4AddressHelper address12;
    address12.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces12;
    interfaces12 = address12.Assign(devices12); // Node 1: 10.1.2.1, Node 2: 10.1.2.2

    // Populate routing tables. This is crucial for Node 1 to act as a router
    // and for Node 0 to reach Node 2 and vice-versa through Node 1.
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Setup UDP Echo Server on Node 2 (the third node)
    uint16_t port = 9; // Standard Echo protocol port
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(2)); // Install on Node 2
    serverApps.Start(Seconds(1.0)); // Server starts at 1 second
    serverApps.Stop(Seconds(10.0));

    // Setup UDP Echo Client on Node 0 (the first node)
    // The client sends packets to the server's IP address (Node 2's IP on the 10.1.2.0 network)
    Ipv4Address serverAddress = interfaces12.GetAddress(1); // Get Node 2's IP address (10.1.2.2)

    UdpEchoClientHelper echoClient(serverAddress, port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(1)); // Send 1 packet
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0))); // Interval between packets (not critical for 1 packet)
    echoClient.SetAttribute("PacketSize", UintegerValue(1024)); // Packet size of 1024 bytes

    ApplicationContainer clientApps = echoClient.Install(nodes.Get(0)); // Install on Node 0
    clientApps.Start(Seconds(2.0)); // Client starts at 2 seconds, after server
    clientApps.Stop(Seconds(10.0));

    // Set simulation duration
    Simulator::Stop(Seconds(11.0));

    // Run the simulation
    Simulator::Run();

    // Clean up simulation resources
    Simulator::Destroy();

    return 0;
}