#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h" // For IPv4 global routing table population

// Define a log component for this script to enable detailed logging if needed.
NS_LOG_COMPONENT_DEFINE("UdpEchoCsmaSimulation");

int main(int argc, char *argv[])
{
    // 1. Command-line arguments setup
    // A boolean flag to determine whether to use IPv6 (default is IPv4).
    bool useIpv6 = false;

    // Create a CommandLine object and add the 'ipv6' argument.
    CommandLine cmd(__FILE__);
    cmd.AddValue("ipv6", "Use IPv6 instead of IPv4 for the simulation (default: false for IPv4)", useIpv6);
    cmd.Parse(argc, argv); // Parse command-line arguments.

    // 2. Logging setup
    // The program description specifies "Allow for additional logging for specific sections."
    // By default, ns-3 modules log only at LOG_LEVEL_ERROR. To see more detailed output,
    // uncomment and enable specific log components here.
    NS_LOG_INFO("Setting up logging components.");
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
    // Uncomment the following lines for more verbose CSMA device and channel logging:
    // LogComponentEnable("CsmaNetDevice", LOG_LEVEL_INFO);
    // LogComponentEnable("CsmaChannel", LOG_LEVEL_INFO);
    // LogComponentEnable("Packet", LOG_LEVEL_INFO); // Useful for packet headers/dumps

    // Set the global time resolution for the simulation.
    Time::SetResolution(NanoSeconds(1));

    // 3. Create nodes
    NS_LOG_INFO("Creating 4 nodes (n0, n1, n2, n3).");
    NodeContainer nodes;
    nodes.Create(4); // Creates n0, n1, n2, n3.

    // 4. Configure CSMA channel and install devices
    NS_LOG_INFO("Configuring CSMA channel attributes (DataRate, Delay, MTU).");
    CsmaHelper csma;
    // Set CSMA channel data rate to 5 Mbps.
    csma.SetChannelAttribute("DataRate", StringValue("5Mbps"));
    // Set CSMA channel propagation delay to 2 milliseconds.
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));
    // Set CSMA NetDevice MTU to 1400 bytes.
    csma.SetDeviceAttribute("Mtu", UintegerValue(1400));

    NS_LOG_INFO("Installing CSMA devices on all 4 nodes.");
    NetDeviceContainer devices = csma.Install(nodes); // Install CSMA devices on all nodes and connect them to the channel.

    // 5. Install IP stack (IPv4 or IPv6) on all nodes
    NS_LOG_INFO("Installing Internet Stack (IPv4 or IPv6) on nodes.");
    InternetStackHelper stack;
    stack.Install(nodes);

    // 6. Assign IP addresses (IPv4 or IPv6 based on command-line argument)
    NS_LOG_INFO("Assigning IP addresses to devices.");
    Ipv4InterfaceContainer ipv4Interfaces;
    Ipv6InterfaceContainer ipv6Interfaces;

    if (useIpv6)
    {
        // If IPv6 is selected, assign IPv6 addresses.
        Ipv6AddressHelper ipv6Addresses;
        // Set the base IPv6 network address and prefix.
        ipv6Addresses.SetBase("2001:db8::", Ipv6Prefix(64));
        ipv6Interfaces = ipv6Addresses.Assign(devices); // Assign addresses to CSMA devices.
        // For a single broadcast segment, no specific IPv6 routing protocol setup is needed.
    }
    else
    {
        // If IPv4 is selected (default), assign IPv4 addresses.
        Ipv4AddressHelper ipv4Addresses;
        // Set the base IPv4 network address and netmask.
        ipv4Addresses.SetBase("10.1.1.0", "255.255.255.0");
        ipv4Interfaces = ipv4Addresses.Assign(devices); // Assign addresses to CSMA devices.

        // Populate global routing tables for IPv4. This is good practice even for a single LAN segment,
        // although direct communication works without it on the same segment.
        NS_LOG_INFO("Populating IPv4 global routing tables.");
        Ipv4GlobalRoutingHelper::PopulateRoutingTables();
    }

    // Get specific node pointers for clarity in application setup.
    Ptr<Node> n0 = nodes.Get(0); // Node 0 is the client.
    Ptr<Node> n1 = nodes.Get(1); // Node 1 is the server.

    // 7. Configure UDP Echo Server on node n1 (port 9)
    NS_LOG_INFO("Setting up UDP Echo Server on node n1, port 9.");
    uint16_t port = 9; // Standard UDP Echo port.
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApps = echoServer.Install(n1); // Install server on node n1.
    serverApps.Start(Seconds(1.0));                           // Start server at 1 second.
    serverApps.Stop(Seconds(11.0));                           // Stop server at 11 seconds (after client finishes).

    // 8. Configure UDP Echo Client on node n0
    NS_LOG_INFO("Setting up UDP Echo Client on node n0.");
    UdpEchoClientHelper echoClient;

    if (useIpv6)
    {
        // Get the IPv6 address of n1's CSMA device.
        // GetObject<Ipv6>() retrieves the IPv6 protocol stack on n1.
        // GetAddress(1, 1) gets the global IPv6 address (interface 1 is CSMA, 0 is link-local, 1 is global).
        // GetLocal() extracts the Ipv6Address from Ipv6InterfaceAddress.
        Ptr<Ipv6> ipv6n1 = n1->GetObject<Ipv6>();
        Ipv6Address serverAddress = ipv6n1->GetAddress(1, 1).GetLocal();
        echoClient.SetRemoteAddress(serverAddress);
    }
    else
    {
        // Get the IPv4 address of n1's CSMA device.
        // GetObject<Ipv4>() retrieves the IPv4 protocol stack on n1.
        // GetAddress(1, 0) gets the IPv4 address (interface 1 is CSMA, 0 is the primary address).
        // GetLocal() extracts the Ipv4Address from Ipv4InterfaceAddress.
        Ptr<Ipv4> ipv4n1 = n1->GetObject<Ipv4>();
        Ipv4Address serverAddress = ipv4n1->GetAddress(1, 0).GetLocal();
        echoClient.SetRemoteAddress(serverAddress);
    }

    echoClient.SetRemotePort(port);
    echoClient.SetAttribute("PacketSize", UintegerValue(1024)); // Client sends 1024-byte packets.
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0))); // Client sends packets at 1-second intervals.
    // Client starts at 2s and stops at 10s. This means it sends packets at 2, 3, 4, 5, 6, 7, 8, 9 seconds.
    // So, it sends 8 packets in total.
    echoClient.SetAttribute("MaxPackets", UintegerValue(8));

    ApplicationContainer clientApps = echoClient.Install(n0); // Install client on node n0.
    clientApps.Start(Seconds(2.0));                           // Client starts sending at 2 seconds.
    clientApps.Stop(Seconds(10.0));                           // Client stops sending at 10 seconds.

    // 9. Enable tracing
    NS_LOG_INFO("Enabling ASCII and PCAP tracing.");
    // ASCII Trace: "udp-echo.tr" for queue events and packet receptions.
    // This will trace all events (Tx, Rx, Queue) for all devices installed on the CSMA channel.
    AsciiTraceHelper asciiTraceHelper;
    Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream("udp-echo.tr");
    // Enable ASCII trace for all devices, including transmit, receive, and queue events.
    csma.EnableAsciiTrace(stream, devices, true, true, true);

    // PCAP Trace: "udp-echo-*.pcap" files for all devices.
    // The 'true' argument ensures that devices are created to support Pcap (if not already).
    csma.EnablePcapAll("udp-echo", true);

    // 10. Run simulation
    NS_LOG_INFO("Running simulation...");
    Simulator::Run(); // Start the simulation.
    NS_LOG_INFO("Simulation finished. Cleaning up.");
    Simulator::Destroy(); // Clean up simulation resources.

    return 0;
}