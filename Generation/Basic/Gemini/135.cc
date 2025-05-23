#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/udp-echo-helper.h"

int main(int argc, char *argv[])
{
    // Enable logging for relevant components to observe packet transmissions and reception
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
    LogComponentEnable("PointToPointNetDevice", LOG_LEVEL_INFO); // Log events on the point-to-point device

    // Create a single ns-3 Node
    NodeContainer nodes;
    nodes.Create(1);

    // Configure Point-to-Point connection properties
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    // Install the Point-to-Point NetDevice on the single node.
    // Although it's a single node, this configures its network interface.
    NetDeviceContainer devices;
    devices = pointToPoint.Install(nodes);

    // Install the Internet stack on the node
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign an IP address to the node's network interface
    // The single node will be assigned the IP address 10.1.1.1
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Set up the UDP Echo Server application on the single node
    uint16_t serverPort = 9; // Standard echo port
    UdpEchoServerHelper echoServer(serverPort);
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(0)); // Install server on the single node
    serverApps.Start(Seconds(1.0));  // Server starts at 1 second
    serverApps.Stop(Seconds(10.0)); // Server stops at 10 seconds

    // Set up the UDP Echo Client application on the same node
    // The client sends packets to the server's address, which is the node's own IP (10.1.1.1)
    UdpEchoClientHelper echoClient(interfaces.GetAddress(0), serverPort);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));    // Send 5 packets
    echoClient.SetAttribute("Interval", TimeValue(Seconds(0.5))); // Send a packet every 0.5 seconds
    echoClient.SetAttribute("PacketSize", UintegerValue(1024)); // Each packet will be 1024 bytes

    ApplicationContainer clientApps = echoClient.Install(nodes.Get(0)); // Install client on the single node
    clientApps.Start(Seconds(2.0));  // Client starts at 2 seconds
    clientApps.Stop(Seconds(9.0));  // Client stops at 9 seconds

    // Set the simulation stop time
    Simulator::Stop(Seconds(11.0)); // Ensure simulation runs long enough for all events

    // Run the simulation
    Simulator::Run();

    // Clean up simulation resources
    Simulator::Destroy();

    return 0;
}