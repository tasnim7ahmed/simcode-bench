#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

// Define NS_LOG_COMPONENT_DEFINE to get logging output if desired
// NS_LOG_COMPONENT_DEFINE ("PointToPointUdpNetAnim");

int main (int argc, char *argv[])
{
    // 1. Configure simulation parameters
    uint32_t packetSize = 1024;  // bytes
    uint32_t numPackets = 5;     // number of packets to send
    uint16_t serverPort = 9;     // UDP Echo port

    // Allow command line arguments to override default parameters
    ns3::CommandLine cmd;
    cmd.AddValue ("PacketSize", "Size of packets in bytes", packetSize);
    cmd.AddValue ("NumPackets", "Number of packets to send", numPackets);
    cmd.AddValue ("ServerPort", "Port number for the UDP server", serverPort);
    cmd.Parse (argc, argv);

    // 2. Create nodes
    ns3::NodeContainer nodes;
    nodes.Create (2); // Creates two nodes

    // 3. Create point-to-point devices and install them on nodes
    ns3::PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute ("DataRate", ns3::StringValue ("5Mbps"));
    pointToPoint.SetChannelAttribute ("Delay", ns3::StringValue ("2ms"));

    ns3::NetDeviceContainer devices;
    devices = pointToPoint.Install (nodes);

    // 4. Install Internet stack (IP, TCP, UDP, etc.) on nodes
    ns3::InternetStackHelper stack;
    stack.Install (nodes);

    // 5. Assign IP addresses to the devices
    ns3::Ipv4AddressHelper ipv4;
    ipv4.SetBase ("10.1.1.0", "255.255.255.0"); // Network 10.1.1.0/24
    ns3::Ipv4InterfaceContainer interfaces = ipv4.Assign (devices);

    // 6. Set up UDP Echo Server on Node 1 (index 1 in NodeContainer)
    ns3::UdpEchoServerHelper echoServer (serverPort);
    ns3::ApplicationContainer serverApps = echoServer.Install (nodes.Get (1)); // Server on node 1
    serverApps.Start (ns3::Seconds (1.0)); // Server starts at 1 second
    serverApps.Stop (ns3::Seconds (10.0)); // Server stops at 10 seconds

    // 7. Set up UDP Echo Client on Node 0 (index 0 in NodeContainer)
    // Client sends packets to the IP address of the server (interfaces.GetAddress(1))
    ns3::UdpEchoClientHelper echoClient (interfaces.GetAddress (1), serverPort);
    echoClient.SetAttribute ("MaxPackets", ns3::UintegerValue (numPackets));
    echoClient.SetAttribute ("Interval", ns3::TimeValue (ns3::Seconds (1.0))); // Send one packet per second
    echoClient.SetAttribute ("PacketSize", ns3::UintegerValue (packetSize));

    ns3::ApplicationContainer clientApps = echoClient.Install (nodes.Get (0)); // Client on node 0
    clientApps.Start (ns3::Seconds (2.0)); // Client starts at 2 seconds (after server is up)
    clientApps.Stop (ns3::Seconds (10.0)); // Client stops at 10 seconds

    // 8. Enable packet tracing
    // Generates pcap files for analyzing network traffic with Wireshark
    pointToPoint.EnablePcapAll ("p2p-udp");

    // 9. Generate NetAnim XML trace file
    // Initializes the animation engine with the specified XML file name
    ns3::AnimationInterface anim ("p2p-udp-netanim.xml");

    // Set constant positions for nodes for better visualization
    // Node 0 at (10, 50) and Node 1 at (90, 50)
    anim.SetConstantPosition (nodes.Get (0), 10.0, 50.0);
    anim.SetConstantPosition (nodes.Get (1), 90.0, 50.0);

    // Packet transmission animations are automatically included when AnimationInterface is used.
    // The link will visually represent the transmission.

    // 10. Run the simulation
    ns3::Simulator::Stop (ns3::Seconds (11.0)); // Stop simulation at 11 seconds
    ns3::Simulator::Run ();
    ns3::Simulator::Destroy ();

    return 0;
}