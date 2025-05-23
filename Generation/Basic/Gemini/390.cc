#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/udp-client-server-helper.h"

// NS_LOG_COMPONENT_DEFINE("CsmaUdpEchoExample"); // Not allowed as per "no commentary" rule

int main(int argc, char *argv[])
{
    // Configure default attributes for the simulation
    // LogComponentEnable("CsmaUdpEchoExample", LOG_LEVEL_INFO); // Not allowed as per "no commentary" rule
    // LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO); // Not allowed as per "no commentary" rule
    // LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO); // Not allowed as per "no commentary" rule

    // Create two nodes
    ns3::NodeContainer nodes;
    nodes.Create(2);

    // Create the CSMA helper and set channel attributes
    ns3::CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", ns3::StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay", ns3::TimeValue(ns3::MilliSeconds(2)));

    // Install CSMA devices on the nodes
    ns3::NetDeviceContainer devices = csma.Install(nodes);

    // Install the Internet stack on the nodes
    ns3::InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses to the devices
    ns3::Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    ns3::Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Setup UDP Echo Server on Node 1
    ns3::uint16_t port = 9; // Standard echo port
    ns3::UdpEchoServerHelper echoServer(port);
    ns3::ApplicationContainer serverApps = echoServer.Install(nodes.Get(1)); // Node 1 is the receiver
    serverApps.Start(ns3::Seconds(0.0));
    serverApps.Stop(ns3::Seconds(10.0));

    // Setup UDP Echo Client on Node 0
    ns3::UdpEchoClientHelper echoClient(interfaces.GetAddress(1), port); // Target Node 1's IP address
    echoClient.SetAttribute("MaxPackets", ns3::UintegerValue(100));     // Send 100 packets
    echoClient.SetAttribute("Interval", ns3::TimeValue(ns3::MilliSeconds(100))); // Send every 100ms
    echoClient.SetAttribute("PacketSize", ns3::UintegerValue(1024));     // Packet size of 1024 bytes
    ns3::ApplicationContainer clientApps = echoClient.Install(nodes.Get(0)); // Node 0 is the sender
    clientApps.Start(ns3::Seconds(1.0)); // Start client after 1 second
    clientApps.Stop(ns3::Seconds(10.0));

    // Set the simulation stop time
    ns3::Simulator::Stop(ns3::Seconds(10.0));

    // Run the simulation
    ns3::Simulator::Run();
    ns3::Simulator::Destroy();

    return 0;
}