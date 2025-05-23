#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/udp-trace-client-helper.h"
#include "ns3/ipv4-global-routing-helper.h"

#include <fstream>
#include <string>
#include <cstdio> // For std::remove

using namespace ns3;

// Command line options
static bool g_useIpv6 = false;
static bool g_enableLog = false;
static std::string g_traceFileName = "udp-trace.dat";

// Function to generate the trace file required by TraceBasedUdpClient
// The format is: time_ms packet_size_bytes
void GenerateTraceFile()
{
    std::ofstream os(g_traceFileName.c_str());
    if (!os.is_open())
    {
        std::cerr << "Error: Could not open trace file for writing: " << g_traceFileName << std::endl;
        exit(1); // Terminate if trace file cannot be created
    }

    // Example trace data: times are relative to the client application's start time
    os << "0 1024" << std::endl;      // At 0ms (relative), send 1024 bytes
    os << "500 512" << std::endl;     // At 500ms (relative), send 512 bytes
    os << "1000 2048" << std::endl;   // At 1000ms (relative), send 2048 bytes
    os << "1500 1200" << std::endl;   // At 1500ms (relative), send 1200 bytes
    os.close();
    if (g_enableLog)
    {
        NS_LOG_INFO("Generated trace file: " << g_traceFileName);
    }
}

int main(int argc, char *argv[])
{
    // Set default time resolution for the simulator
    Time::SetResolution(NanoSeconds(1));

    // Configure command line parsing
    CommandLine cmd(__FILE__);
    cmd.AddValue("useIpv6", "Set to true to use IPv6 addressing (default: IPv4)", g_useIpv6);
    cmd.AddValue("enableLog", "Set to true to enable detailed ns-3 logging (default: false)", g_enableLog);
    cmd.Parse(argc, argv);

    // Conditionally enable ns-3 logging components
    if (g_enableLog)
    {
        LogComponentEnable("TraceBasedUdpClient", LOG_LEVEL_INFO);
        LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
        LogComponentEnable("CsmaNetDevice", LOG_LEVEL_INFO);
        LogComponentEnable("Ipv4L3Protocol", LOG_LEVEL_INFO);
        LogComponentEnable("Ipv6L3Protocol", LOG_LEVEL_INFO);
    }

    // Generate the dummy trace file that the UDP client will read from
    GenerateTraceFile();

    // Create two nodes: n0 and n1
    NodeContainer nodes;
    nodes.Create(2); // nodes.Get(0) is n0, nodes.Get(1) is n1

    // Configure CSMA channel properties
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("5Mbps"));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));
    csma.SetDeviceAttribute("Mtu", UintegerValue(1500));

    // Install CSMA devices on nodes and connect them via the channel
    NetDeviceContainer devices = csma.Install(nodes);

    // Install the Internet stack (IPv4 or IPv6 based on command line option)
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses to the devices
    Address serverAddress; // This will hold the IP address of n1 (the server)
    uint16_t serverPort = 9; // Standard discard port, or any other for UDP

    if (g_useIpv6)
    {
        if (g_enableLog) { NS_LOG_INFO("Configuring IPv6 addressing."); }
        Ipv6AddressHelper ipv6;
        ipv6.SetBase("2001:db8:1::", Ipv6Prefix(64)); // Common IPv6 documentation prefix
        Ipv6InterfaceContainer ipv6Ifaces = ipv6.Assign(devices);
        serverAddress = ipv6Ifaces.GetAddress(1); // Get IPv6 address of n1 (devices.Get(1))
    }
    else
    {
        if (g_enableLog) { NS_LOG_INFO("Configuring IPv4 addressing."); }
        Ipv4AddressHelper ipv4;
        ipv4.SetBase("10.1.1.0", "255.255.255.0"); // Private IPv4 range
        Ipv4InterfaceContainer ipv4Ifaces = ipv4.Assign(devices);
        serverAddress = ipv4Ifaces.GetAddress(1); // Get IPv4 address of n1 (devices.Get(1))
    }

    // Configure and install UDP server on n1 (nodes.Get(1))
    // The server will start at 1 second and stop at 10 seconds.
    UdpServerHelper serverHelper(serverPort);
    ApplicationContainer serverApps = serverHelper.Install(nodes.Get(1)); // n1 is the server
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // Configure and install UDP client on n0 (nodes.Get(0))
    // The client will start at 2 seconds and stop at 10 seconds.
    // Packets (size and send time) will be drawn from the generated trace file.
    TraceBasedUdpClientHelper clientHelper(serverAddress, serverPort);
    clientHelper.SetAttribute("Filename", StringValue(g_traceFileName));
    ApplicationContainer clientApps = clientHelper.Install(nodes.Get(0)); // n0 is the client
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Populate routing tables for IPv4. For a simple single CSMA segment,
    // this isn't strictly necessary as devices are on the same subnet,
    // but it's good practice for general setups involving more complex topologies.
    // For IPv6, Neighbor Discovery Protocol (NDP) handles on-link communication.
    if (!g_useIpv6)
    {
        Ipv4GlobalRoutingHelper::PopulateRoutingTables();
    }

    // Set simulation duration and run the simulation
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    // Clean up the generated trace file
    std::remove(g_traceFileName.c_str());
    if (g_enableLog)
    {
        NS_LOG_INFO("Cleaned up trace file: " << g_traceFileName);
    }

    return 0;
}