#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

// NS_LOG_COMPONENT_DEFINE ("SimplePointToPoint"); // Not required by the prompt, but useful for debugging

int
main(int argc, char* argv[])
{
    // Enable logging for UDP echo client and server applications
    // NS_LOG_INFO ("Create nodes."); // Not required by the prompt
    // LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO); // Not required by the prompt
    // LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO); // Not required by the prompt

    NodeContainer nodes;
    nodes.Create(2);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices;
    devices = pointToPoint.Install(nodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Set up UDP Echo Server on Node 0
    UdpEchoServerHelper echoServer(9); // Port 9
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(0));
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(10.0));

    // Set up UDP Echo Client on Node 1
    Ptr<Node> clientNode = nodes.Get(1);
    Ptr<Node> serverNode = nodes.Get(0);
    Ipv4Address serverAddress = interfaces.GetAddress(0); // Get IP address of Node 0's interface

    UdpEchoClientHelper echoClient(serverAddress, 9); // Server IP and port 9
    echoClient.SetAttribute("MaxPackets", UintegerValue(1));     // Send a single packet
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));   // Packet size 1024 bytes
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0))); // Interval (doesn't matter much for 1 packet)

    ApplicationContainer clientApps = echoClient.Install(clientNode);
    clientApps.Start(Seconds(1.0)); // Client starts at 1 second
    clientApps.Stop(Seconds(10.0));

    Simulator::Stop(Seconds(10.0)); // Simulation runs for 10 seconds
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}