#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

// NS_LOG_COMPONENT_DEFINE("SimplePointToPointSimulation"); // Not explicitly requested to output logs

int
main(int argc, char* argv[])
{
    // Enable logging for UDP applications (optional, but good for debugging)
    // LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    // LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create two nodes
    ns3::NodeContainer nodes;
    nodes.Create(2);

    // Configure Point-to-Point link
    ns3::PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", ns3::StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", ns3::StringValue("2ms"));

    // Install Point-to-Point devices on the nodes
    ns3::NetDeviceContainer devices = pointToPoint.Install(nodes);

    // Install the Internet stack on the nodes
    ns3::InternetStackHelper internet;
    internet.Install(nodes);

    // Assign IPv4 addresses
    ns3::Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0"); // 10.1.1.0/24 subnet
    ns3::Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    // Set up UDP Echo Server on the second node (node 1)
    ns3::UdpEchoServerHelper echoServer(9); // Port 9
    ns3::ApplicationContainer serverApps = echoServer.Install(nodes.Get(1));
    serverApps.Start(ns3::Seconds(1.0));   // Server starts at 1 second
    serverApps.Stop(ns3::Seconds(10.0));  // Server stops at 10 seconds

    // Set up UDP Echo Client on the first node (node 0)
    ns3::UdpEchoClientHelper echoClient(interfaces.GetAddress(1), 9); // Server address and port
    echoClient.SetAttribute("MaxPackets", ns3::UintegerValue(5));    // Five packets
    echoClient.SetAttribute("PacketSize", ns3::UintegerValue(512));  // 512 bytes per packet
    echoClient.SetAttribute("Interval", ns3::TimeValue(ns3::Seconds(1.0))); // Interval between packets

    ns3::ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
    clientApps.Start(ns3::Seconds(2.0));   // Client starts at 2 seconds (after server is up)
    clientApps.Stop(ns3::Seconds(10.0));  // Client stops at 10 seconds

    // Set simulation stop time
    ns3::Simulator::Stop(ns3::Seconds(10.0));

    // Run the simulation
    ns3::Simulator::Run();
    ns3::Simulator::Destroy();

    return 0;
}