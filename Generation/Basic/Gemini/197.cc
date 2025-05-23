#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

// Define a logging component for this simulation
NS_LOG_COMPONENT_DEFINE ("WiredTcpSimulation");

int
main (int argc, char *argv[])
{
    // Configure default attributes for Point-to-Point devices
    Config::SetDefault ("ns3::PointToPointNetDevice::DataRate", StringValue ("5Mbps"));
    Config::SetDefault ("ns3::PointToPointNetDevice::Delay", StringValue ("2ms"));

    // Create three nodes
    NodeContainer nodes;
    nodes.Create (3); // Node 0, Node 1 (router), Node 2

    // Create Point-to-Point Helper
    PointToPointHelper p2p;

    // Create the first P2P link: Node 0 <-> Node 1
    NetDeviceContainer devices1;
    devices1 = p2p.Install (nodes.Get (0), nodes.Get (1));

    // Create the second P2P link: Node 1 <-> Node 2
    NetDeviceContainer devices2;
    devices2 = p2p.Install (nodes.Get (1), nodes.Get (2));

    // Install Internet Stack on all nodes
    InternetStackHelper stack;
    stack.Install (nodes);

    // Assign IP Addresses to the first link (10.1.1.0/24)
    Ipv4AddressHelper address1;
    address1.SetBase ("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces1;
    interfaces1 = address1.Assign (devices1); // Node 0: 10.1.1.1, Node 1: 10.1.1.2

    // Assign IP Addresses to the second link (10.1.2.0/24)
    Ipv4AddressHelper address2;
    address2.SetBase ("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces2;
    interfaces2 = address2.Assign (devices2); // Node 1: 10.1.2.1, Node 2: 10.1.2.2

    // Populate routing tables (enables Node 1 to act as a router)
    Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

    // Configure TCP Server (PacketSink) on Node 2
    uint16_t sinkPort = 9; // Echo server port
    PacketSinkHelper packetSinkHelper ("ns3::TcpSocketFactory",
                                       InetSocketAddress (Ipv4Address::GetAny (), sinkPort));
    ApplicationContainer sinkApps = packetSinkHelper.Install (nodes.Get (2));
    sinkApps.Start (Seconds (0.0)); // Server starts at simulation beginning
    sinkApps.Stop (Seconds (10.0)); // Server stops at simulation end

    // Configure TCP Client (BulkSend) on Node 0
    // Target Node 2's IP address on the second link (10.1.2.2)
    Ipv4Address serverAddress = interfaces2.GetAddress (1); // 10.1.2.2
    BulkSendHelper bulkSendHelper ("ns3::TcpSocketFactory",
                                   InetSocketAddress (serverAddress, sinkPort));
    // Send unlimited bytes
    bulkSendHelper.SetAttribute ("MaxBytes", UintegerValue (0));
    ApplicationContainer clientApps = bulkSendHelper.Install (nodes.Get (0));
    clientApps.Start (Seconds (1.0)); // Client starts 1 second after server
    clientApps.Stop (Seconds (9.0));  // Client stops 1 second before server

    // Enable PCAP tracing on all Point-to-Point devices
    // This will generate .pcap files (e.g., wired-tcp-simulation-0-0.pcap) for packet analysis
    p2p.EnablePcapAll ("wired-tcp-simulation");

    // Enable ASCII tracing on all Point-to-Point devices
    // This will generate .tr files (e.g., wired-tcp-simulation-0-0.tr)
    // The .tr files log each transmitted and received packet event,
    // which can be used to derive the number of packets on each node.
    p2p.EnableAsciiAll ("wired-tcp-simulation");

    // Set simulation stop time
    Simulator::Stop (Seconds (10.0));

    // Run the simulation
    Simulator::Run ();

    // Clean up simulation resources
    Simulator::Destroy ();

    return 0;
}