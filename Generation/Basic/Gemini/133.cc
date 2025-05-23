#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/rip-module.h"

#include <iostream>

// For NS_LOG_COMPONENT_DEFINE
#include "ns3/log.h"

NS_LOG_COMPONENT_DEFINE("SplitHorizonRipSimulation");

using namespace ns3;

// A helper function to print the routing table for a given node.
void PrintRoutingTable(Ptr<Node> node, Time time)
{
    Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
    NS_ASSERT(ipv4 != nullptr);

    Ptr<RipRoutingProtocol> rip = DynamicCast<RipRoutingProtocol>(ipv4->GetRoutingProtocol());
    NS_ASSERT(rip != nullptr);

    std::cout << "Node " << node->GetId() << " Routing Table at " << time.GetSeconds() << "s:" << std::endl;
    rip->PrintRoutingTable(std::cout); // Print the RIP routing table to stdout.
    std::cout << "---------------------------------------------------------" << std::endl;
}

int main(int argc, char *argv[])
{
    // Enable logging for RIP and other relevant modules to observe behavior.
    // LOG_LEVEL_DEBUG for RipRoutingProtocol is key to see Split Horizon in action.
    LogComponentEnable("SplitHorizonRipSimulation", LOG_LEVEL_INFO);
    LogComponentEnable("RipRoutingProtocol", LOG_LEVEL_DEBUG);
    LogComponentEnable("Ipv4L3Protocol", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Command line arguments (none defined, but good practice to include)
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    // 1. Create nodes: Node A (0), Node B (1), Node C (2)
    NS_LOG_INFO("Creating three nodes: A, B, C.");
    NodeContainer nodes;
    nodes.Create(3);

    // 2. Configure PointToPoint links
    NS_LOG_INFO("Configuring PointToPoint links for A-B and B-C.");
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("10ms"));

    NetDeviceContainer devicesAB;
    devicesAB = p2p.Install(nodes.Get(0), nodes.Get(1)); // Link between Node A and Node B

    NetDeviceContainer devicesBC;
    devicesBC = p2p.Install(nodes.Get(1), nodes.Get(2)); // Link between Node B and Node C

    // 3. Install InternetStack with RIP routing
    NS_LOG_INFO("Installing Internet Stack with RIP routing on all nodes.");
    RipHelper ripRouting;

    // Use RIPv2 for enhanced features like subnet masks and authentication (though not used here).
    ripRouting.SetRipVersion(RipHelper::RIP_V2);

    // Explicitly set Split Horizon. SPLIT_HORIZON is the default, but explicitly setting it
    // makes the intention clear for this demonstration.
    // Other options: NO_SPLIT_HORIZON (to see loops), SPLIT_HORIZON_POISON_REVERSE.
    ripRouting.SetSplitHorizon(RipHelper::SplitHorizon::SPLIT_HORIZON);

    // Install RIP on all nodes. This also installs Ipv4 on the nodes.
    ripRouting.Install(nodes);

    // 4. Assign IP addresses to the network interfaces
    NS_LOG_INFO("Assigning IP addresses to interfaces.");
    Ipv4AddressHelper address;

    // Assign IP addresses to the A-B link (10.0.0.0/24)
    address.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer interfacesAB;
    interfacesAB = address.Assign(devicesAB); // Node A: 10.0.0.1, Node B: 10.0.0.2

    // Assign IP addresses to the B-C link (10.0.1.0/24)
    address.SetBase("10.0.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfacesBC;
    interfacesBC = address.Assign(devicesBC); // Node B: 10.0.1.1, Node C: 10.0.1.2

    // Note: Node B (nodes.Get(1)) has two interfaces (10.0.0.2 and 10.0.1.1).
    // RipHelper automatically enables RIP on all active interfaces unless excluded.

    // 5. Setup Applications (UdpEchoClient/Server)
    NS_LOG_INFO("Setting up UdpEchoClient/Server applications.");
    uint16_t port = 9; // Standard echo port

    // Server on Node C (Node 2)
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(2));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(25.0));

    // Client on Node A (Node 0), targeting Node C's IP address (10.0.1.2)
    UdpEchoClientHelper echoClient(interfacesBC.GetAddress(1), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(3));      // Send 3 packets
    echoClient.SetAttribute("Interval", TimeValue(Seconds(3.0))); // 3 seconds between packets
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));   // 1024 byte packets
    ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));  // Start after server and initial routing announcements
    clientApps.Stop(Seconds(25.0));

    // 6. Enable PCAP tracing for all PointToPoint devices
    NS_LOG_INFO("Enabling PCAP tracing for all point-to-point links.");
    p2p.EnablePcapAll("split-horizon-rip");

    // 7. Schedule routing table prints to observe routing table convergence and state
    // These prints help confirm that routes are correctly established and stable.
    // The RipRoutingProtocol debug logs will show the actual filtering of advertisements.
    Simulator::Schedule(Seconds(0.1), &PrintRoutingTable, nodes.Get(0), Seconds(0.1));
    Simulator::Schedule(Seconds(0.1), &PrintRoutingTable, nodes.Get(1), Seconds(0.1));
    Simulator::Schedule(Seconds(0.1), &PrintRoutingTable, nodes.Get(2), Seconds(0.1));

    Simulator::Schedule(Seconds(5.0), &PrintRoutingTable, nodes.Get(0), Seconds(5.0));
    Simulator::Schedule(Seconds(5.0), &PrintRoutingTable, nodes.Get(1), Seconds(5.0));
    Simulator::Schedule(Seconds(5.0), &PrintRoutingTable, nodes.Get(2), Seconds(5.0));

    Simulator::Schedule(Seconds(10.0), &PrintRoutingTable, nodes.Get(0), Seconds(10.0));
    Simulator::Schedule(Seconds(10.0), &PrintRoutingTable, nodes.Get(1), Seconds(10.0));
    Simulator::Schedule(Seconds(10.0), &PrintRoutingTable, nodes.Get(2), Seconds(10.0));

    // Run simulation
    NS_LOG_INFO("Starting simulation...");
    Simulator::Stop(Seconds(25.0)); // Stop after a reasonable time for convergence and traffic
    Simulator::Run();
    NS_LOG_INFO("Simulation finished.");
    Simulator::Destroy();

    return 0;
}