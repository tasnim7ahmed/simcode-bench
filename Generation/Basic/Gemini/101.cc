#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"

int main()
{
    // Set default time resolution to 1 nanosecond
    Time::SetResolution(NanoSeconds(1));

    // Create 4 nodes: n0, n1, n2, n3
    NodeContainer nodes;
    nodes.Create(4);

    // Point-to-Point Helper configuration
    PointToPointHelper p2pHelper;
    p2pHelper.SetQueue("ns3::DropTailQueue"); // Use DropTail queues as requested

    // Link n0 to n2
    p2pHelper.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2pHelper.SetChannelAttribute("Delay", StringValue("2ms"));
    NetDeviceContainer devices02 = p2pHelper.Install(nodes.Get(0), nodes.Get(2));

    // Link n1 to n2
    p2pHelper.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2pHelper.SetChannelAttribute("Delay", StringValue("2ms"));
    NetDeviceContainer devices12 = p2pHelper.Install(nodes.Get(1), nodes.Get(2));

    // Link n2 to n3
    p2pHelper.SetDeviceAttribute("DataRate", StringValue("1.5Mbps"));
    p2pHelper.SetChannelAttribute("Delay", StringValue("10ms"));
    NetDeviceContainer devices23 = p2pHelper.Install(nodes.Get(2), nodes.Get(3));

    // Install Internet Stack on all nodes
    InternetStackHelper internet;
    internet.Install(nodes);

    // Assign IP Addresses to interfaces
    Ipv4AddressHelper ipv4;

    ipv4.SetBase("10.1.1.0", "255.255.255.0"); // For n0-n2 link
    Ipv4InterfaceContainer interfaces02 = ipv4.Assign(devices02);

    ipv4.SetBase("10.1.2.0", "255.255.255.0"); // For n1-n2 link
    Ipv4InterfaceContainer interfaces12 = ipv4.Assign(devices12);

    ipv4.SetBase("10.1.3.0", "255.255.255.0"); // For n2-n3 link
    Ipv4InterfaceContainer interfaces23 = ipv4.Assign(devices23);

    // Populate routing tables using global routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // --- CBR/UDP Traffic Setup ---
    uint16_t udpPort0 = 4000; // Port for n0 -> n3 traffic
    uint16_t udpPort1 = 4001; // Port for n3 -> n1 traffic
    uint32_t packetSize = 210; // bytes
    double dataRateKbps = 448.0; // kbps
    double intervalSeconds = static_cast<double>(packetSize * 8) / (dataRateKbps * 1000); // Calculate interval for given rate and packet size

    // CBR from n0 to n3
    // Install PacketSink (server) on n3
    PacketSinkHelper udpSinkHelper0("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), udpPort0));
    ApplicationContainer serverApps0 = udpSinkHelper0.Install(nodes.Get(3));
    serverApps0.Start(Seconds(0.0));
    serverApps0.Stop(Seconds(10.0)); // Run for the duration of the simulation

    // Install UdpClient (client) on n0, targeting n3's IP on the n2-n3 link
    UdpClientHelper udpClient0(interfaces23.GetAddress(1), udpPort0);
    udpClient0.SetAttribute("MaxPackets", StringValue("0")); // Send indefinitely
    udpClient0.SetAttribute("Interval", TimeValue(Seconds(intervalSeconds)));
    udpClient0.SetAttribute("PacketSize", UintegerValue(packetSize));
    ApplicationContainer clientApps0 = udpClient0.Install(nodes.Get(0));
    clientApps0.Start(Seconds(1.0));
    clientApps0.Stop(Seconds(10.0));

    // CBR from n3 to n1
    // Install PacketSink (server) on n1
    PacketSinkHelper udpSinkHelper1("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), udpPort1));
    ApplicationContainer serverApps1 = udpSinkHelper1.Install(nodes.Get(1));
    serverApps1.Start(Seconds(0.0));
    serverApps1.Stop(Seconds(10.0)); // Run for the duration of the simulation

    // Install UdpClient (client) on n3, targeting n1's IP on the n1-n2 link
    UdpClientHelper udpClient1(interfaces12.GetAddress(0), udpPort1);
    udpClient1.SetAttribute("MaxPackets", StringValue("0")); // Send indefinitely
    udpClient1.SetAttribute("Interval", TimeValue(Seconds(intervalSeconds)));
    udpClient1.SetAttribute("PacketSize", UintegerValue(packetSize));
    ApplicationContainer clientApps1 = udpClient1.Install(nodes.Get(3));
    clientApps1.Start(Seconds(1.0));
    clientApps1.Stop(Seconds(10.0));

    // --- FTP/TCP Flow Setup from n0 to n3 ---
    uint16_t tcpPort = 5000;

    // Install PacketSink (server) on n3
    PacketSinkHelper tcpSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), tcpPort));
    ApplicationContainer tcpSinkApps = tcpSinkHelper.Install(nodes.Get(3));
    tcpSinkApps.Start(Seconds(0.0));
    tcpSinkApps.Stop(Seconds(10.0)); // Run for the duration of the simulation

    // Install BulkSend (client) on n0, targeting n3's IP on the n2-n3 link
    BulkSendHelper tcpSourceHelper("ns3::TcpSocketFactory", InetSocketAddress(interfaces23.GetAddress(1), tcpPort));
    tcpSourceHelper.SetAttribute("MaxBytes", UintegerValue(0)); // Send indefinitely until stop time
    ApplicationContainer tcpSourceApps = tcpSourceHelper.Install(nodes.Get(0));
    tcpSourceApps.Start(Seconds(1.2));
    tcpSourceApps.Stop(Seconds(1.35));

    // --- Tracing ---
    AsciiTraceHelper ascii;
    p2pHelper.EnableAsciiAll(ascii.CreateFileStream("simple-global-routing.tr"));

    // Set simulation stop time (to allow all traffic to run for their specified durations)
    Simulator::Stop(Seconds(10.0));

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}