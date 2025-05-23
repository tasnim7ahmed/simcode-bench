#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/udp-echo-helper.h"

int main(int argc, char *argv[])
{
    // Configure logging for UdpEchoClient and UdpEchoServer applications
    ns3::LogComponentEnable("UdpEchoClientApplication", ns3::LOG_LEVEL_INFO);
    ns3::LogComponentEnable("UdpEchoServerApplication", ns3::LOG_LEVEL_INFO);

    // Create two nodes for client and server
    ns3::NodeContainer nodes;
    nodes.Create(2);

    // Set up a point-to-point connection between the nodes
    ns3::PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", ns3::StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", ns3::StringValue("2ms"));
    ns3::NetDeviceContainer devices = pointToPoint.Install(nodes);

    // Install the Internet stack on both nodes
    ns3::InternetStackHelper internet;
    internet.Install(nodes);

    // Assign IP addresses to the devices
    ns3::Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    ns3::Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    // Set up UdpEchoServer on the first node (server)
    uint16_t port = 9; // Standard discard port
    ns3::UdpEchoServerHelper echoServer(port);
    ns3::ApplicationContainer serverApps = echoServer.Install(nodes.Get(0));
    serverApps.Start(ns3::Seconds(0.0));   // Server starts at the beginning of simulation
    serverApps.Stop(ns3::Seconds(10.0)); // Server runs for the duration of the simulation

    // Set up UdpEchoClient on the second node (client)
    ns3::Ipv4Address serverAddress = interfaces.GetAddress(0); // Get the IP address of the server node
    ns3::UdpEchoClientHelper echoClient(serverAddress, port);
    echoClient.SetAttribute("MaxPackets", ns3::UintegerValue(10)); // Send 10 packets
    echoClient.SetAttribute("Interval", ns3::TimeValue(ns3::Seconds(1.0))); // Send one packet every second
    echoClient.SetAttribute("PacketSize", ns3::UintegerValue(1024)); // Packet size of 1024 bytes

    ns3::ApplicationContainer clientApps = echoClient.Install(nodes.Get(1));
    clientApps.Start(ns3::Seconds(1.0)); // Client starts sending packets at 1 second
    clientApps.Stop(ns3::Seconds(10.0)); // Client stops sending at 10 seconds

    // Set the simulation duration
    ns3::Simulator::Stop(ns3::Seconds(10.0));

    // Run the simulation
    ns3::Simulator::Run();

    // Clean up simulation resources
    ns3::Simulator::Destroy();

    return 0;
}