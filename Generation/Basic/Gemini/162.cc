#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/ipv4-global-routing-helper.h"

int main(int argc, char* argv[])
{
    // Set up time resolution
    ns3::Time::SetResolution(ns3::NanoSeconds);

    // Enable logging for relevant components to log packet transmissions
    ns3::LogComponentEnable("OnOffApplication", ns3::LOG_LEVEL_INFO);
    ns3::LogComponentEnable("PacketSink", ns3::LOG_LEVEL_INFO);
    ns3::LogComponentEnable("TcpSocketBase", ns3::LOG_LEVEL_INFO);
    ns3::LogComponentEnable("PointToPointNetDevice", ns3::LOG_LEVEL_INFO);

    // Create 4 nodes for the ring topology
    ns3::NodeContainer nodes;
    nodes.Create(4);

    // Install the Internet stack on all nodes
    ns3::InternetStackHelper internet;
    internet.Install(nodes);

    // Configure Point-to-Point links
    ns3::PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", ns3::StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", ns3::StringValue("1ms"));

    // Connect nodes in a ring topology
    // Link 0-1
    ns3::NetDeviceContainer devices01 = p2p.Install(nodes.Get(0), nodes.Get(1));
    // Link 1-2
    ns3::NetDeviceContainer devices12 = p2p.Install(nodes.Get(1), nodes.Get(2));
    // Link 2-3
    ns3::NetDeviceContainer devices23 = p2p.Install(nodes.Get(2), nodes.Get(3));
    // Link 3-0 (completes the ring)
    ns3::NetDeviceContainer devices30 = p2p.Install(nodes.Get(3), nodes.Get(0));

    // Assign IP addresses to each link segment
    ns3::Ipv4AddressHelper ipv4;

    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    ns3::Ipv4InterfaceContainer if01 = ipv4.Assign(devices01);

    ipv4.SetBase("10.1.2.0", "255.255.255.0");
    ns3::Ipv4InterfaceContainer if12 = ipv4.Assign(devices12);

    ipv4.SetBase("10.1.3.0", "255.255.255.0");
    ns3::Ipv4InterfaceContainer if23 = ipv4.Assign(devices23);

    ipv4.SetBase("10.1.4.0", "255.255.255.0");
    ns3::Ipv4InterfaceContainer if30 = ipv4.Assign(devices30);

    // Populate routing tables for inter-subnet communication in the ring
    ns3::Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Set up TCP server application (Node 2)
    uint16_t port = 9; // Standard echo port, can be any unused port

    // Server IP address: Node 2's address on the link 1-2 (10.1.2.2)
    // The PacketSink helper will listen on all interfaces (Ipv4Address::GetAny())
    ns3::PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", ns3::InetSocketAddress(ns3::Ipv4Address::GetAny(), port));
    ns3::ApplicationContainer serverApps = sinkHelper.Install(nodes.Get(2));
    serverApps.Start(ns3::Seconds(0.0)); // Server starts at the beginning
    serverApps.Stop(ns3::Seconds(10.0)); // Server runs for 10 seconds

    // Set up TCP client application (Node 0)
    // Target server's IP address: Node 2's address on the 10.1.2.0 subnet
    ns3::Address serverAddress(ns3::InetSocketAddress(if12.GetAddress(1), port));
    ns3::OnOffApplicationHelper onoff("ns3::TcpSocketFactory", serverAddress);

    // Configure OnOffApplication for a persistent connection
    onoff.SetAttribute("OnTime", ns3::StringValue("ns3::ConstantRandomVariable[Constant=1]")); // Always On
    onoff.SetAttribute("OffTime", ns3::StringValue("ns3::ConstantRandomVariable[Constant=0]")); // Never Off
    onoff.SetAttribute("PacketSize", ns3::UintegerValue(1024)); // 1 KB packet size
    onoff.SetAttribute("DataRate", ns3::StringValue("1Mbps")); // Client sends at 1 Mbps persistently

    ns3::ApplicationContainer clientApps = onoff.Install(nodes.Get(0));
    clientApps.Start(ns3::Seconds(1.0)); // Client starts 1 second after simulation start
    clientApps.Stop(ns3::Seconds(10.0)); // Client stops at 10 seconds

    // Configure NetAnim for visualization
    ns3::AnimationInterface anim("ring_topology_netanim.xml");
    // Set constant positions for nodes to visualize the ring
    anim.SetConstantPosition(nodes.Get(0), 10.0, 50.0);
    anim.SetConstantPosition(nodes.Get(1), 50.0, 90.0);
    anim.SetConstantPosition(nodes.Get(2), 90.0, 50.0);
    anim.SetConstantPosition(nodes.Get(3), 50.0, 10.0);

    // Run the simulation
    ns3::Simulator::Stop(ns3::Seconds(11.0)); // Stop slightly after applications finish
    ns3::Simulator::Run();
    ns3::Simulator::Destroy();

    return 0;
}