#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/ipv6-global-routing-helper.h"

using namespace ns3;

int main(int argc, char* argv[])
{
    // Default configuration for IP version and logging
    bool ipv4Enable = true;
    bool ipv6Enable = false;
    bool logging = false;

    // Command line options to configure the simulation
    CommandLine cmd(__FILE__);
    cmd.AddValue("ipv4", "Enable IPv4 addressing (default: true).", ipv4Enable);
    cmd.AddValue("ipv6", "Enable IPv6 addressing (default: false).", ipv6Enable);
    cmd.AddValue("logging", "Enable logging of simulation events (default: false).", logging);
    cmd.Parse(argc, argv);

    // Validate IP version selection
    if (ipv4Enable && ipv6Enable)
    {
        NS_FATAL_ERROR("Error: Cannot enable both IPv4 and IPv6. Please select only one.");
    }
    if (!ipv4Enable && !ipv6Enable)
    {
        NS_LOG_INFO("No IP version specified. Defaulting to IPv4.");
        ipv4Enable = true;
    }

    // Configure logging based on command line argument
    if (logging)
    {
        LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
        LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
        LogComponentEnable("CsmaHelper", LOG_LEVEL_INFO);
        LogComponentEnable("PacketSink", LOG_LEVEL_INFO);
    }

    // Create two nodes for the LAN
    NodeContainer nodes;
    nodes.Create(2); // n0 and n1

    // Configure CSMA channel properties
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay", StringValue("2ms"));

    // Install CSMA devices on nodes
    NetDeviceContainer devices = csma.Install(nodes);

    // Install internet stack on nodes
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses based on the selected version
    Address serverAddress; // Will hold the IP address and port of the server
    uint16_t serverPort = 9; // Echo port for UDP

    if (ipv4Enable)
    {
        Ipv4AddressHelper ipv4;
        ipv4.SetBase("10.1.1.0", "255.255.255.0");
        Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);
        // Get the address of n1 (index 1) for the server
        serverAddress = InetSocketAddress(interfaces.GetAddress(1), serverPort);
    }
    else // ipv6Enable must be true
    {
        Ipv6AddressHelper ipv6;
        ipv6.SetBase("2001:db8::", Ipv6Prefix(64));
        Ipv6InterfaceContainer interfaces = ipv6.Assign(devices);
        // Get the address of n1 (index 1) for the server
        serverAddress = Inet6SocketAddress(interfaces.GetAddress(1), serverPort);
    }

    // Install UDP Server application on node n1
    UdpServerHelper server(serverPort);
    ApplicationContainer serverApps = server.Install(nodes.Get(1)); // Install on n1
    serverApps.Start(Seconds(1.0)); // Server starts at 1 second
    serverApps.Stop(Seconds(10.0)); // Server stops at 10 seconds (end of simulation)

    // Install UDP Client application on node n0
    UdpClientHelper client(serverAddress); // Server address already includes the port
    client.SetAttribute("MaxPackets", UintegerValue(320)); // Max 320 packets
    client.SetAttribute("Interval", TimeValue(MilliSeconds(50))); // 50 ms interval
    client.SetAttribute("PacketSize", UintegerValue(1024)); // 1024 byte packets

    ApplicationContainer clientApps = client.Install(nodes.Get(0)); // Install on n0
    clientApps.Start(Seconds(2.0)); // Client starts at 2 seconds
    clientApps.Stop(Seconds(10.0)); // Client stops at 10 seconds or when MaxPackets reached

    // Set simulation duration
    Simulator::Stop(Seconds(10.0));

    // Run the simulation
    Simulator::Run();

    // Clean up simulation resources
    Simulator::Destroy();

    return 0;
}