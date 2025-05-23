#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

// Define a log component for the simulation script
NS_LOG_COMPONENT_DEFINE("UdpClientServerSimulation");

int
main(int argc, char* argv[])
{
    // Enable logging for UDP applications
    // NS_LOG_INFO will print info messages about UDP server/client
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("UdpClientServerSimulation", LOG_LEVEL_INFO);

    // Create two nodes
    NS_LOG_INFO("Creating two nodes...");
    NodeContainer nodes;
    nodes.Create(2);

    // Configure point-to-point link attributes
    NS_LOG_INFO("Configuring point-to-point link...");
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // Install network devices on nodes
    NetDeviceContainer devices = p2p.Install(nodes);

    // Install internet stack (IPv4, ARP, etc.) on nodes
    NS_LOG_INFO("Installing InternetStack...");
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IPv4 addresses to the network devices
    NS_LOG_INFO("Assigning IPv4 addresses...");
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Set up UDP Server on Node 0
    NS_LOG_INFO("Setting up UDP Server on Node 0...");
    uint16_t port = 9; // Echo port
    UdpServerHelper server(port);
    ApplicationContainer serverApps = server.Install(nodes.Get(0));
    serverApps.Start(Seconds(1.0)); // Server starts at 1 second
    serverApps.Stop(Seconds(10.0)); // Server stops at 10 seconds

    // Set up UDP Client on Node 1
    NS_LOG_INFO("Setting up UDP Client on Node 1...");
    // The client sends to the server's IPv4 address on Node 0
    Ipv4Address serverAddress = interfaces.GetAddress(0);
    UdpClientHelper client(serverAddress, port);
    client.SetAttribute("MaxPackets", UintegerValue(10)); // Send 10 packets
    client.SetAttribute("PacketSize", UintegerValue(1024)); // Packet size 1024 bytes
    client.SetAttribute("Interval", TimeValue(Seconds(1.0))); // 1 packet per second

    ApplicationContainer clientApps = client.Install(nodes.Get(1));
    clientApps.Start(Seconds(2.0)); // Client starts at 2 seconds
    clientApps.Stop(Seconds(10.0)); // Client stops at 10 seconds

    // Configure simulation stop time
    NS_LOG_INFO("Running simulation for 10 seconds...");
    Simulator::Stop(Seconds(10.0));

    // Run the simulation
    Simulator::Run();

    // Clean up simulation resources
    Simulator::Destroy();
    NS_LOG_INFO("Simulation finished.");

    return 0;
}