#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create four nodes
    NodeContainer nodes;
    nodes.Create(4);

    // Configure PointToPointHelper attributes
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // Create ring topology and install devices
    // Store NetDeviceContainer for IP assignment later
    NetDeviceContainer devices[4]; // devices[0] = N0-N1, devices[1] = N1-N2, etc.

    for (uint32_t i = 0; i < 4; ++i)
    {
        Ptr<Node> nodeA = nodes.Get(i);
        Ptr<Node> nodeB = nodes.Get((i + 1) % 4); // Connects 0-1, 1-2, 2-3, 3-0
        devices[i] = p2p.Install(nodeA, nodeB);
    }

    // Install internet stack on all nodes
    InternetStackHelper internet;
    internet.Install(nodes);

    // Assign IP addresses within 192.9.39.0/24
    // Each point-to-point link will get a /30 subnet from this /24 range.
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("192.9.39.0", "255.255.255.0");

    Ipv4InterfaceContainer interfaces[4]; // interfaces[0] for N0-N1 link, etc.

    for (uint32_t i = 0; i < 4; ++i)
    {
        interfaces[i] = ipv4.Assign(devices[i]);
        ipv4.NewNetwork(); // Move to the next subnet for the next link
    }

    // Populate routing tables for all nodes
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Application Setup (UDP Echo Client-Server)
    // Ensure each node acts as both client and server at least once,
    // and only one pair of client-server applications runs at any given time.

    uint16_t port = 9; // Standard echo port
    double currentStartTime = 1.0;
    const double appDuration = 5.0; // Time each client-server pair will run
    const double interAppDelay = 1.0; // Time gap between consecutive applications

    // Define IP addresses for easier access to remote targets
    // For N0 (client) -> N1 (server), N0's client targets N1's IP on their shared link (devices[0])
    Ipv4Address N1_IP_on_N0_N1_link = interfaces[0].GetAddress(1);
    // For N1 (client) -> N2 (server), N1's client targets N2's IP on their shared link (devices[1])
    Ipv4Address N2_IP_on_N1_N2_link = interfaces[1].GetAddress(1);
    // For N2 (client) -> N3 (server), N2's client targets N3's IP on their shared link (devices[2])
    Ipv4Address N3_IP_on_N2_N3_link = interfaces[2].GetAddress(1);
    // For N3 (client) -> N0 (server), N3's client targets N0's IP on their shared link (devices[3])
    Ipv4Address N0_IP_on_N3_N0_link = interfaces[3].GetAddress(1);

    // Application 1: N0 (client) -> N1 (server)
    UdpEchoServerHelper serverHelper1(port);
    ApplicationContainer serverApps1 = serverHelper1.Install(nodes.Get(1)); // Server on N1
    serverApps1.Start(Seconds(currentStartTime));
    serverApps1.Stop(Seconds(currentStartTime + appDuration));

    UdpEchoClientHelper clientHelper1(N1_IP_on_N0_N1_link, port);
    clientHelper1.SetAttribute("MaxPackets", UintegerValue(4));
    clientHelper1.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    clientHelper1.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps1 = clientHelper1.Install(nodes.Get(0)); // Client on N0
    clientApps1.Start(Seconds(currentStartTime));
    clientApps1.Stop(Seconds(currentStartTime + appDuration));

    currentStartTime += appDuration + interAppDelay;

    // Application 2: N1 (client) -> N2 (server)
    UdpEchoServerHelper serverHelper2(port);
    ApplicationContainer serverApps2 = serverHelper2.Install(nodes.Get(2)); // Server on N2
    serverApps2.Start(Seconds(currentStartTime));
    serverApps2.Stop(Seconds(currentStartTime + appDuration));

    UdpEchoClientHelper clientHelper2(N2_IP_on_N1_N2_link, port);
    clientHelper2.SetAttribute("MaxPackets", UintegerValue(4));
    clientHelper2.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    clientHelper2.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps2 = clientHelper2.Install(nodes.Get(1)); // Client on N1
    clientApps2.Start(Seconds(currentStartTime));
    clientApps2.Stop(Seconds(currentStartTime + appDuration));

    currentStartTime += appDuration + interAppDelay;

    // Application 3: N2 (client) -> N3 (server)
    UdpEchoServerHelper serverHelper3(port);
    ApplicationContainer serverApps3 = serverHelper3.Install(nodes.Get(3)); // Server on N3
    serverApps3.Start(Seconds(currentStartTime));
    serverApps3.Stop(Seconds(currentStartTime + appDuration));

    UdpEchoClientHelper clientHelper3(N3_IP_on_N2_N3_link, port);
    clientHelper3.SetAttribute("MaxPackets", UintegerValue(4));
    clientHelper3.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    clientHelper3.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps3 = clientHelper3.Install(nodes.Get(2)); // Client on N2
    clientApps3.Start(Seconds(currentStartTime));
    clientApps3.Stop(Seconds(currentStartTime + appDuration));

    currentStartTime += appDuration + interAppDelay;

    // Application 4: N3 (client) -> N0 (server)
    UdpEchoServerHelper serverHelper4(port);
    ApplicationContainer serverApps4 = serverHelper4.Install(nodes.Get(0)); // Server on N0
    serverApps4.Start(Seconds(currentStartTime));
    serverApps4.Stop(Seconds(currentStartTime + appDuration));

    UdpEchoClientHelper clientHelper4(N0_IP_on_N3_N0_link, port);
    clientHelper4.SetAttribute("MaxPackets", UintegerValue(4));
    clientHelper4.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    clientHelper4.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps4 = clientHelper4.Install(nodes.Get(3)); // Client on N3
    clientApps4.Start(Seconds(currentStartTime));
    clientApps4.Stop(Seconds(currentStartTime + appDuration));

    // Set total simulation time to allow all applications to complete plus a small buffer
    Simulator::Stop(Seconds(currentStartTime + 1.0));

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}