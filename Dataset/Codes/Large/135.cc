#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("UDPEchoExample");

int 
main(int argc, char *argv[])
{
    // Log settings
    LogComponentEnable("UDPEchoExample", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create a node
    NodeContainer nodes;
    nodes.Create(1); // One node for both client and server

    // Point-to-point link configuration (though it's only one node)
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    // Install the network stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");

    // Create a loopback network device (since only one node is involved)
    Ipv4InterfaceContainer interfaces;
    interfaces = address.Assign(pointToPoint.Install(nodes));

    // Create and set up UDP Echo Server
    uint16_t port = 9; // Echo server port number
    UdpEchoServerHelper echoServer(port);

    ApplicationContainer serverApp = echoServer.Install(nodes.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Create and set up UDP Echo Client
    uint32_t packetSize = 1024; // Packet size
    uint32_t nPackets = 10;     // Number of packets to send
    double interval = 1.0;      // Time between packets (1 second)
    
    UdpEchoClientHelper echoClient(interfaces.GetAddress(0), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(nPackets));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(interval)));
    echoClient.SetAttribute("PacketSize", UintegerValue(packetSize));

    ApplicationContainer clientApp = echoClient.Install(nodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Run the simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}

