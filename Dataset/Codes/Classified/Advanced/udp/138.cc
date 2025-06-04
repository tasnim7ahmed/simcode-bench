#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("UdpEchoFourNodesExample");

int 
main(int argc, char *argv[])
{
    // Log settings
    LogComponentEnable("UdpEchoFourNodesExample", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create four nodes
    NodeContainer nodes;
    nodes.Create(4); // Four nodes: n0 -> n1 -> n2 -> n3

    // Configure point-to-point link between nodes n0 and n1
    PointToPointHelper pointToPoint1;
    pointToPoint1.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint1.SetChannelAttribute("Delay", StringValue("2ms"));

    // Configure point-to-point link between nodes n1 and n2
    PointToPointHelper pointToPoint2;
    pointToPoint2.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint2.SetChannelAttribute("Delay", StringValue("2ms"));

    // Configure point-to-point link between nodes n2 and n3
    PointToPointHelper pointToPoint3;
    pointToPoint3.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint3.SetChannelAttribute("Delay", StringValue("2ms"));

    // Install devices on the nodes and the links between them
    NetDeviceContainer devices1, devices2, devices3;
    devices1 = pointToPoint1.Install(nodes.Get(0), nodes.Get(1)); // Link n0 -> n1
    devices2 = pointToPoint2.Install(nodes.Get(1), nodes.Get(2)); // Link n1 -> n2
    devices3 = pointToPoint3.Install(nodes.Get(2), nodes.Get(3)); // Link n2 -> n3

    // Install Internet stack (TCP/IP stack) on the nodes
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses to the nodes' links
    Ipv4AddressHelper address;
    
    // IP address for the link between n0 and n1
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces1 = address.Assign(devices1);

    // IP address for the link between n1 and n2
    address.SetBase("10.2.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces2 = address.Assign(devices2);

    // IP address for the link between n2 and n3
    address.SetBase("10.3.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces3 = address.Assign(devices3);

    // Enable routing on nodes n1 and n2 (routers)
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Create and set up UDP Echo Server on node n3 (server)
    uint16_t port = 9; // Echo server port number
    UdpEchoServerHelper echoServer(port);

    ApplicationContainer serverApp = echoServer.Install(nodes.Get(3)); // Server is node n3
    serverApp.Start(Seconds(1.0)); // Start server at time = 1 second
    serverApp.Stop(Seconds(10.0)); // Stop server at time = 10 seconds

    // Create and set up UDP Echo Client on node n0 (client)
    uint32_t packetSize = 1024;   // Packet size
    uint32_t nPackets = 10;       // Number of packets to send
    double interval = 1.0;        // Interval between packets (1 second)
    
    UdpEchoClientHelper echoClient(interfaces3.GetAddress(1), port); // Server's IP (n3)
    echoClient.SetAttribute("MaxPackets", UintegerValue(nPackets));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(interval)));
    echoClient.SetAttribute("PacketSize", UintegerValue(packetSize));

    ApplicationContainer clientApp = echoClient.Install(nodes.Get(0)); // Client is node n0
    clientApp.Start(Seconds(2.0)); // Start client at time = 2 seconds
    clientApp.Stop(Seconds(10.0)); // Stop client at time = 10 seconds

    // Run the simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}

