#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

int main(int argc, char *argv[])
{
    // Set default attributes for PointToPoint links
    ns3::Config::SetDefault("ns3::PointToPointNetDevice::DataRate", ns3::StringValue("10Mbps"));
    ns3::Config::SetDefault("ns3::PointToPointChannel::Delay", ns3::StringValue("2ms"));

    // Create nodes for the star topology: Node 0 (central), Node 1, Node 2, Node 3 (peripherals)
    ns3::NodeContainer starNodes;
    starNodes.Create(4);

    // Create nodes for the bus topology: Node 4, Node 5, Node 6
    ns3::NodeContainer busNodes;
    busNodes.Create(3);

    // Combine all nodes into a single container for easier installation of InternetStack and FlowMonitor
    ns3::NodeContainer allNodes;
    allNodes.Add(starNodes);
    allNodes.Add(busNodes);

    // Install the Internet stack on all nodes
    ns3::InternetStackHelper stack;
    stack.Install(allNodes);

    // Helper for Point-to-Point links
    ns3::PointToPointHelper p2p;

    // Create links for the Star Topology
    // Node 0 (starNodes.Get(0)) is the central node
    ns3::NetDeviceContainer d_n0n1 = p2p.Install(starNodes.Get(0), starNodes.Get(1)); // Node 0 <-> Node 1
    ns3::NetDeviceContainer d_n0n2 = p2p.Install(starNodes.Get(0), starNodes.Get(2)); // Node 0 <-> Node 2
    ns3::NetDeviceContainer d_n0n3 = p2p.Install(starNodes.Get(0), starNodes.Get(3)); // Node 0 <-> Node 3

    // Create links for the Bus Topology
    // Node 4 (busNodes.Get(0)), Node 5 (busNodes.Get(1)), Node 6 (busNodes.Get(2))
    ns3::NetDeviceContainer d_n4n5 = p2p.Install(busNodes.Get(0), busNodes.Get(1)); // Node 4 <-> Node 5
    ns3::NetDeviceContainer d_n5n6 = p2p.Install(busNodes.Get(1), busNodes.Get(2)); // Node 5 <-> Node 6

    // Assign IP addresses to the network devices
    ns3::Ipv4AddressHelper ipv4;

    // Star Topology Subnets
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    ns3::Ipv4InterfaceContainer i_n0n1 = ipv4.Assign(d_n0n1); // Node 0 gets 10.1.1.1, Node 1 gets 10.1.1.2

    ipv4.SetBase("10.1.2.0", "255.255.255.0");
    ns3::Ipv4InterfaceContainer i_n0n2 = ipv4.Assign(d_n0n2); // Node 0 gets 10.1.2.1, Node 2 gets 10.1.2.2

    ipv4.SetBase("10.1.3.0", "255.255.255.0");
    ns3::Ipv4InterfaceContainer i_n0n3 = ipv4.Assign(d_n0n3); // Node 0 gets 10.1.3.1, Node 3 gets 10.1.3.2

    // Bus Topology Subnets
    ipv4.SetBase("10.1.4.0", "255.255.255.0");
    ns3::Ipv4InterfaceContainer i_n4n5 = ipv4.Assign(d_n4n5); // Node 4 gets 10.1.4.1, Node 5 gets 10.1.4.2

    ipv4.SetBase("10.1.5.0", "255.255.255.0");
    ns3::Ipv4InterfaceContainer i_n5n6 = ipv4.Assign(d_n5n6); // Node 5 gets 10.1.5.1, Node 6 gets 10.1.5.2

    // Populate routing tables for inter-subnet communication
    // Ipv4GlobalRoutingHelper is installed by default with InternetStackHelper
    // and populates routing tables if called after IP addresses are assigned.
    ns3::Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Deploy UDP Echo Server on Node 0 (central node in star topology)
    uint16_t port = 9; // Standard Echo Protocol port
    ns3::UdpEchoServerHelper echoServer(port);
    ns3::ApplicationContainer serverApps = echoServer.Install(starNodes.Get(0)); // Node 0 is starNodes.Get(0)
    serverApps.Start(ns3::Seconds(1.0));
    serverApps.Stop(ns3::Seconds(20.0));

    // Deploy UDP Echo Clients on Node 4 and Node 5 from the bus topology
    // Clients will send packets to Node 0's IP address (we choose 10.1.1.1 for consistency)
    ns3::Ipv4Address serverAddress = i_n0n1.GetAddress(0); // This is 10.1.1.1 (Node 0's IP on N0-N1 link)

    // UDP Echo Client on Node 4 (busNodes.Get(0))
    ns3::UdpEchoClientHelper echoClient1(serverAddress, port);
    echoClient1.SetAttribute("MaxPackets", ns3::UintegerValue(100));     // Send 100 packets
    echoClient1.SetAttribute("Interval", ns3::TimeValue(ns3::Seconds(0.2))); // Send every 0.2 seconds
    echoClient1.SetAttribute("PacketSize", ns3::UintegerValue(1024));    // 1024 bytes per packet
    ns3::ApplicationContainer clientApps1 = echoClient1.Install(busNodes.Get(0)); // Node 4
    clientApps1.Start(ns3::Seconds(2.0));
    clientApps1.Stop(ns3::Seconds(20.0));

    // UDP Echo Client on Node 5 (busNodes.Get(1))
    ns3::UdpEchoClientHelper echoClient2(serverAddress, port);
    echoClient2.SetAttribute("MaxPackets", ns3::UintegerValue(100));     // Send 100 packets
    echoClient2.SetAttribute("Interval", ns3::TimeValue(ns3::Seconds(0.2))); // Send every 0.2 seconds
    echoClient2.SetAttribute("PacketSize", ns3::UintegerValue(1024));    // 1024 bytes per packet
    ns3::ApplicationContainer clientApps2 = echoClient2.Install(busNodes.Get(1)); // Node 5
    clientApps2.Start(ns3::Seconds(2.5)); // Start slightly later to differentiate traffic
    clientApps2.Stop(ns3::Seconds(20.0));

    // Configure FlowMonitor to track performance metrics
    ns3::FlowMonitorHelper flowMonitor;
    ns3::Ptr<ns3::FlowMonitor> monitor = flowMonitor.InstallAll(); // Monitor all devices on all nodes

    // Set simulation duration
    ns3::Simulator::Stop(ns3::Seconds(20.0));

    // Run the simulation
    ns3::Simulator::Run();

    // Collect and serialize FlowMonitor statistics to an XML file
    monitor->SerializeToXml("hybrid-topology-flowmonitor.xml", true, true);

    // Clean up simulation resources
    ns3::Simulator::Destroy();

    return 0;
}