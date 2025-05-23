#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"

// Do not add any commentary or markdown fences.
// This file should be directly compilable as a .cc file.

int main(int argc, char *argv[])
{
    // Set up default attributes for PointToPoint links
    ns3::PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", ns3::StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", ns3::StringValue("2ms"));

    // Create 4 nodes
    ns3::NodeContainer nodes;
    nodes.Create(4); // Node 0, Node 1, Node 2, Node 3

    // Create point-to-point links between nodes
    // Link 0-1
    ns3::NodeContainer n0n1 = ns3::NodeContainer(nodes.Get(0), nodes.Get(1));
    ns3::NetDeviceContainer d0d1 = p2p.Install(n0n1);

    // Link 1-2
    ns3::NodeContainer n1n2 = ns3::NodeContainer(nodes.Get(1), nodes.Get(2));
    ns3::NetDeviceContainer d1d2 = p2p.Install(n1n2);

    // Link 2-3
    ns3::NodeContainer n2n3 = ns3::NodeContainer(nodes.Get(2), nodes.Get(3));
    ns3::NetDeviceContainer d2d3 = p2p.Install(n2n3);

    // Install internet stack on all nodes
    ns3::InternetStackHelper internet;
    internet.Install(nodes);

    // Assign IP addresses to each network segment
    ns3::Ipv4AddressHelper ipv4;

    // Segment 0-1
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    ns3::Ipv4InterfaceContainer i0i1 = ipv4.Assign(d0d1);

    // Segment 1-2
    ipv4.SetBase("10.1.2.0", "255.255.255.0");
    ns3::Ipv4InterfaceContainer i1i2 = ipv4.Assign(d1d2);

    // Segment 2-3
    ipv4.SetBase("10.1.3.0", "255.255.255.0");
    ns3::Ipv4InterfaceContainer i2i3 = ipv4.Assign(d2d3);

    // Populate routing tables
    ns3::Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Configure UDP traffic from Node 0 to Node 3
    uint16_t port = 9; // Discard port

    // Create UDP Server on Node 3
    ns3::UdpServerHelper server(port);
    ns3::ApplicationContainer serverApps = server.Install(nodes.Get(3));
    serverApps.Start(ns3::Seconds(1.0)); // Server starts at 1 second
    serverApps.Stop(ns3::Seconds(9.0));  // Server stops at 9 seconds

    // Create UDP Client on Node 0 sending to Node 3
    // Node 3's IP address on the 10.1.3.0 segment is i2i3.GetAddress(1)
    ns3::UdpClientHelper client(i2i3.GetAddress(1), port);
    client.SetAttribute("MaxPackets", ns3::UintegerValue(1000)); // Send 1000 packets
    client.SetAttribute("Interval", ns3::TimeValue(ns3::Seconds(0.01))); // Send a packet every 0.01 seconds (100 pkts/sec)
    client.SetAttribute("PacketSize", ns3::UintegerValue(1024)); // 1024 bytes per packet

    ns3::ApplicationContainer clientApps = client.Install(nodes.Get(0));
    clientApps.Start(ns3::Seconds(2.0)); // Client starts at 2 seconds
    clientApps.Stop(ns3::Seconds(8.0));  // Client stops at 8 seconds

    // Enable PCAP tracing for all point-to-point devices
    p2p.EnablePcapAll("line-topology-udp");

    // Set simulation stop time and run
    ns3::Simulator::Stop(ns3::Seconds(10.0));
    ns3::Simulator::Run();
    ns3::Simulator::Destroy();

    return 0;
}