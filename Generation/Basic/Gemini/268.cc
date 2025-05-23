#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"

// Define NS_LOG_COMPONENT_DEFINE to avoid linker errors if not used
// NS_LOG_COMPONENT_DEFINE("FourNodeCsmaEcho"); // Not required by the prompt, so omitting.

int main() {
    // Set time resolution
    Time::SetResolution(NanoSeconds(1));

    // Create 4 nodes
    NodeContainer nodes;
    nodes.Create(4);

    // Install the Internet stack on all nodes
    InternetStackHelper internet;
    internet.Install(nodes);

    // Create the CSMA helper and set channel attributes
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay", TimeValue(MicroSeconds(6.56)));

    // Connect nodes to the CSMA channel
    NetDeviceContainer devices;
    devices = csma.Install(nodes);

    // Assign IPv4 addresses to the devices
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    // --- UDP Echo Server Setup ---
    // The last node (index 3) runs the UDP echo server on port 9
    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(3));
    serverApps.Start(Seconds(1.0)); // Server starts at 1 second
    serverApps.Stop(Seconds(10.0)); // Server stops at 10 seconds

    // --- UDP Echo Client Setup ---
    // The first node (index 0) acts as a client
    // Get the IPv4 address of the server (last node)
    Ipv4Address serverAddress = interfaces.GetAddress(3);
    uint16_t serverPort = 9; // Port for the echo server

    UdpEchoClientHelper echoClient(serverAddress, serverPort);
    echoClient.SetAttribute("MaxPackets", UintegerValue(10));       // Send 10 packets
    echoClient.SetAttribute("Interval", TimeValue(Seconds(0.5)));   // Send a packet every 0.5 seconds
    echoClient.SetAttribute("PacketSize", UintegerValue(512));      // Packet size of 512 bytes

    ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
    clientApps.Start(Seconds(1.5)); // Client starts 0.5 seconds after server
    clientApps.Stop(Seconds(10.0));  // Client stops at 10 seconds

    // Set up tracing (optional, but good for debugging/visualization)
    // csma.EnablePcapAll("four-node-csma-echo");

    // Configure simulation stop time
    Simulator::Stop(Seconds(10.0));

    // Run the simulation
    Simulator::Run();

    // Clean up
    Simulator::Destroy();

    return 0;
}