#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

// Use ns3 namespace directly for convenience
using namespace ns3;

int main(int argc, char *argv[])
{
    // Enable logging for relevant components to observe packet flow
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
    LogComponentEnable("Ipv4L3Protocol", LOG_LEVEL_INFO); // For IP layer transmission/reception details

    // Command line parsing for ns-3 attributes (optional, but good practice)
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    // Create six nodes (Node 0 to Node 5)
    NodeContainer nodes;
    nodes.Create(6);

    // Configure Point-to-Point Helper with specified data rate and delay
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // NetDeviceContainers for each point-to-point link
    NetDeviceContainer devices01, devices12, devices23, devices34, devices45;

    // Install NetDevices for each link
    devices01 = p2p.Install(nodes.Get(0), nodes.Get(1)); // Node 0 -- Node 1
    devices12 = p2p.Install(nodes.Get(1), nodes.Get(2)); // Node 1 -- Node 2
    devices23 = p2p.Install(nodes.Get(2), nodes.Get(3)); // Node 2 -- Node 3
    devices34 = p2p.Install(nodes.Get(3), nodes.Get(4)); // Node 3 -- Node 4
    devices45 = p2p.Install(nodes.Get(4), nodes.Get(5)); // Node 4 -- Node 5

    // Install the Internet stack on all nodes (includes IPv4, TCP, UDP etc.)
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses to each link, using different /30 subnets
    Ipv4AddressHelper address;
    Ipv4InterfaceContainer interfaces01, interfaces12, interfaces23, interfaces34, interfaces45;

    address.SetBase("10.0.0.0", "255.255.255.252"); // Link 0-1
    interfaces01 = address.Assign(devices01);
    // Node 0: 10.0.0.1, Node 1: 10.0.0.2

    address.SetBase("10.0.1.0", "255.255.255.252"); // Link 1-2
    interfaces12 = address.Assign(devices12);
    // Node 1: 10.0.1.1, Node 2: 10.0.1.2

    address.SetBase("10.0.2.0", "255.255.255.252"); // Link 2-3
    interfaces23 = address.Assign(devices23);
    // Node 2: 10.0.2.1, Node 3: 10.0.2.2

    address.SetBase("10.0.3.0", "255.255.255.252"); // Link 3-4
    interfaces34 = address.Assign(devices34);
    // Node 3: 10.0.3.1, Node 4: 10.0.3.2

    address.SetBase("10.0.4.0", "255.255.255.252"); // Link 4-5 (Server Link)
    interfaces45 = address.Assign(devices45);
    // Node 4: 10.0.4.1, Node 5: 10.0.4.2 (This is the server's IP address)

    // Populate routing tables. This is crucial for nodes 1, 2, 3, 4 to act as routers.
    // It uses a global routing protocol (like RIP or OSPF) to discover routes.
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Set up UDP Echo Server on Node 5
    // The server will listen on port 9
    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(5));
    serverApps.Start(Seconds(1.0)); // Server starts at 1 second
    serverApps.Stop(Seconds(10.0)); // Server stops at 10 seconds

    // Set up UDP Echo Client on Node 0
    // Client sends to Node 5's IP address (10.0.4.2) and port 9
    // interfaces45.GetAddress(1) gets the IP of the second device in devices45, which is node 5's IP.
    UdpEchoClientHelper echoClient(interfaces45.GetAddress(1), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));    // Send 5 packets
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0))); // Send one packet per second
    echoClient.SetAttribute("PacketSize", UintegerValue(1024)); // Packet size of 1024 bytes

    ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0)); // Client starts at 2 seconds
    clientApps.Stop(Seconds(10.0)); // Client stops at 10 seconds

    // Enable PCAP tracing for all devices on all links.
    // This will generate .pcap files (e.g., udp-echo-chain-0-0.pcap for node 0's device 0)
    // which can be analyzed with tools like Wireshark.
    p2p.EnablePcapAll("udp-echo-chain");

    // Set the overall simulation stop time
    Simulator::Stop(Seconds(11.0));

    // Run the simulation
    Simulator::Run();

    // Clean up simulation resources
    Simulator::Destroy();

    return 0;
}