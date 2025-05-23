#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/udp-echo-helper.h"

int main(int argc, char *argv[])
{
    // Create two nodes for the Point-to-Point network
    ns3::NodeContainer nodes;
    nodes.Create(2); // Node 0 (client), Node 1 (server)

    // Configure Point-to-Point link properties
    ns3::PointToPointHelper p2pHelper;
    p2pHelper.SetDeviceAttribute("DataRate", ns3::StringValue("5Mbps"));
    p2pHelper.SetChannelAttribute("Delay", ns3::StringValue("2ms"));

    // Install Point-to-Point devices on the nodes
    ns3::NetDeviceContainer devices;
    devices = p2pHelper.Install(nodes);

    // Install the Internet stack on the nodes
    ns3::InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses to the Point-to-Point interfaces
    ns3::Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0"); // Network: 10.1.1.0/24
    ns3::Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Get the IP address of the server node (Node 1)
    ns3::Ipv4Address serverAddress = interfaces.GetAddress(1);

    // Configure and install the UDP Echo Server application on Node 1
    ns3::UdpEchoServerHelper echoServer(9); // Listen on port 9
    ns3::ApplicationContainer serverApps = echoServer.Install(nodes.Get(1)); // Install on Node 1
    serverApps.Start(ns3::Seconds(0.0));
    serverApps.Stop(ns3::Seconds(10.0)); // Run for the entire simulation duration

    // Configure and install the UDP Echo Client application on Node 0
    ns3::UdpEchoClientHelper echoClient(serverAddress, 9); // Send to serverAddress on port 9
    echoClient.SetAttribute("MaxPackets", ns3::UintegerValue(10));      // Send 10 packets
    echoClient.SetAttribute("PacketSize", ns3::UintegerValue(1024));     // Packet size of 1024 bytes
    echoClient.SetAttribute("Interval", ns3::TimeValue(ns3::Seconds(1.0))); // Send every 1 second

    ns3::ApplicationContainer clientApps = echoClient.Install(nodes.Get(0)); // Install on Node 0
    clientApps.Start(ns3::Seconds(0.0));
    clientApps.Stop(ns3::Seconds(10.0)); // Run for the entire simulation duration

    // Set the simulation stop time
    ns3::Simulator::Stop(ns3::Seconds(10.0));

    // Run the simulation
    ns3::Simulator::Run();

    // Clean up simulation resources
    ns3::Simulator::Destroy();

    return 0;
}