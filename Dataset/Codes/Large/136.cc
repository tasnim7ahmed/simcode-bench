#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("UdpEchoExample");

int 
main(int argc, char *argv[])
{
    // Log settings
    LogComponentEnable("UdpEchoExample", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create two nodes
    NodeContainer nodes;
    nodes.Create(2); // Two nodes, one for client and one for server

    // Configure point-to-point link between the nodes
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    // Install devices on the nodes and the link between them
    NetDeviceContainer devices;
    devices = pointToPoint.Install(nodes);

    // Install Internet stack (TCP/IP stack) on the nodes
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses to the nodes
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Create and set up UDP Echo Server on node 1 (server)
    uint16_t port = 9; // Echo server port number
    UdpEchoServerHelper echoServer(port);

    ApplicationContainer serverApp = echoServer.Install(nodes.Get(1)); // Server is node 1
    serverApp.Start(Seconds(1.0)); // Start server at time = 1 second
    serverApp.Stop(Seconds(10.0)); // Stop server at time = 10 seconds

    // Create and set up UDP Echo Client on node 0 (client)
    uint32_t packetSize = 1024;   // Packet size
    uint32_t nPackets = 10;       // Number of packets to send
    double interval = 1.0;        // Interval between packets (1 second)
    
    UdpEchoClientHelper echoClient(interfaces.GetAddress(1), port); // Server's IP (node 1)
    echoClient.SetAttribute("MaxPackets", UintegerValue(nPackets));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(interval)));
    echoClient.SetAttribute("PacketSize", UintegerValue(packetSize));

    ApplicationContainer clientApp = echoClient.Install(nodes.Get(0)); // Client is node 0
    clientApp.Start(Seconds(2.0)); // Start client at time = 2 seconds
    clientApp.Stop(Seconds(10.0)); // Stop client at time = 10 seconds

    // Run the simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}

