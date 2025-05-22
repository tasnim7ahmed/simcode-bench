#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/internet-apps-module.h"
#include <cmath> // For ceil

using namespace ns3;

int main (int argc, char *argv[])
{
    // Default simulation parameters
    bool useIpv6 = false;
    uint32_t packetSize = 1024;
    double clientStartTime = 2.0;
    double clientStopTime = 10.0;
    double clientInterval = 1.0; // 1-second intervals as per description
    uint16_t echoPort = 9;

    // Enable logging for specific components for additional log outputs
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
    LogComponentEnable("CsmaNetDevice", LOG_LEVEL_INFO);

    // Parse command line arguments
    CommandLine cmd(__FILE__);
    cmd.AddValue("ipv6", "Use IPv6 instead of IPv4", useIpv6);
    cmd.AddValue("packetSize", "Size of UDP echo packet in bytes (default: 1024)", packetSize);
    cmd.AddValue("clientStartTime", "Time for client to start sending packets (default: 2.0s)", clientStartTime);
    cmd.AddValue("clientStopTime", "Time for client to stop sending packets (default: 10.0s)", clientStopTime);
    cmd.Parse(argc, argv);

    // Validate client start/stop times
    if (clientStopTime < clientStartTime)
    {
        NS_FATAL_ERROR("Client stop time must be greater than or equal to client start time.");
    }

    // Create four nodes (n0, n1, n2, n3)
    NodeContainer nodes;
    nodes.Create(4);

    // Configure and install CSMA devices
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("5Mbps"));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));
    csma.SetDeviceAttribute("Mtu", UintegerValue(1400));
    NetDeviceContainer devices = csma.Install(nodes);

    // Install Internet stack on all nodes
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses based on user choice (IPv4 or IPv6)
    Ipv4InterfaceContainer ipv4Interfaces;
    Ipv6InterfaceContainer ipv6Interfaces;

    if (useIpv6)
    {
        Ipv6AddressHelper ipv6;
        ipv6.SetBase("2001:db8::", Ipv6Prefix(64));
        ipv6Interfaces = ipv6.Assign(devices);
    }
    else
    {
        Ipv4AddressHelper ipv4;
        ipv4.SetBase("10.1.1.0", "255.255.255.0");
        ipv4Interfaces = ipv4.Assign(devices);
    }

    // Setup UDP Echo Server on node n1
    UdpEchoServerHelper echoServer(echoPort);
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(1)); // n1
    serverApps.Start(Seconds(1.0)); // Start server before client
    serverApps.Stop(Seconds(clientStopTime + 1.0)); // Stop server after client activity finishes

    // Setup UDP Echo Client on node n0
    Ptr<Node> clientNode = nodes.Get(0); // n0

    // Calculate number of packets to send based on duration and interval
    uint32_t numPacketsToSend = 0;
    if (clientStopTime > clientStartTime)
    {
        numPacketsToSend = (uint32_t)ceil((clientStopTime - clientStartTime) / clientInterval);
    }
    // If clientStartTime == clientStopTime, numPacketsToSend remains 0, meaning no packets are sent.

    if (useIpv6)
    {
        Ipv6Address serverAddress = ipv6Interfaces.GetAddress(1); // Address of n1
        UdpEchoClientHelper echoClient(serverAddress, echoPort);
        echoClient.SetAttribute("MaxPackets", UintegerValue(numPacketsToSend));
        echoClient.SetAttribute("Interval", TimeValue(Seconds(clientInterval)));
        echoClient.SetAttribute("PacketSize", UintegerValue(packetSize));
        ApplicationContainer clientApps = echoClient.Install(clientNode);
        clientApps.Start(Seconds(clientStartTime));
        clientApps.Stop(Seconds(clientStopTime));
    }
    else
    {
        Ipv4Address serverAddress = ipv4Interfaces.GetAddress(1); // Address of n1
        UdpEchoClientHelper echoClient(serverAddress, echoPort);
        echoClient.SetAttribute("MaxPackets", UintegerValue(numPacketsToSend));
        echoClient.SetAttribute("Interval", TimeValue(Seconds(clientInterval)));
        echoClient.SetAttribute("PacketSize", UintegerValue(packetSize));
        ApplicationContainer clientApps = echoClient.Install(clientNode);
        clientApps.Start(Seconds(clientStartTime));
        clientApps.Stop(Seconds(clientStopTime));
    }

    // Enable tracing
    // Ascii trace for queue events and packet receptions
    AsciiTraceHelper ascii;
    csma.EnableAsciiAll(ascii.CreateFileStream("udp-echo.tr"));
    // Pcap tracing for all CSMA devices
    csma.EnablePcapAll("udp-echo", true); // true for promiscuous mode

    // Run simulation
    // Ensure simulation runs long enough for all client packets to be sent and echoed back.
    double totalSimTime = clientStopTime + 2.0; 
    Simulator::Stop(Seconds(totalSimTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}