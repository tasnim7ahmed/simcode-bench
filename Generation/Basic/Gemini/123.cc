/*
 * This program sets up a simple network with four nodes connected in a square topology.
 * It uses point-to-point links and configures OSPF as the routing protocol.
 * IP addresses are assigned to the nodes, and UDP communication is set up between
 * two of the nodes (N0 to N2). Packet tracing is enabled, creating a pcap trace
 * file for each point-to-point link. Routing tables are displayed at the end
 * of the simulation.
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ospf-helper.h"
#include "ns3/ipv4-global-routing-helper.h" // For OSPF setup convenience

// Define a log component for this program
NS_LOG_COMPONENT_DEFINE("OspfSquareTopology");

int
main(int argc, char* argv[])
{
    // Set up default values for data rate and delay
    Config::SetDefault("ns3::PointToPointNetDevice::DataRate", StringValue("5Mbps"));
    Config::SetDefault("ns3::PointToPointChannel::Delay", StringValue("2ms"));
    Config::SetDefault("ns3::DropTailQueue::MaxBytes", StringValue("100000"));

    // Enable logging for OSPF
    LogComponentEnable("OspfRouting", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create four nodes
    NS_LOG_INFO("Creating nodes.");
    NodeContainer nodes;
    nodes.Create(4); // N0, N1, N2, N3

    // Install Internet stack on all nodes
    NS_LOG_INFO("Installing Internet stack.");
    InternetStackHelper internet;
    ospf.Install(nodes); // Installs OSPF-enabled InternetStack

    // Helper for point-to-point links
    PointToPointHelper p2p;

    // Define IP address helper
    Ipv4AddressHelper ipv4;

    // Connect nodes in a square topology and assign IP addresses
    // Link N0-N1
    NS_LOG_INFO("Creating link N0-N1.");
    NetDeviceContainer devices01 = p2p.Install(nodes.Get(0), nodes.Get(1));
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer iface01 = ipv4.Assign(devices01);

    // Link N1-N2
    NS_LOG_INFO("Creating link N1-N2.");
    NetDeviceContainer devices12 = p2p.Install(nodes.Get(1), nodes.Get(2));
    ipv4.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer iface12 = ipv4.Assign(devices12);

    // Link N2-N3
    NS_LOG_INFO("Creating link N2-N3.");
    NetDeviceContainer devices23 = p2p.Install(nodes.Get(2), nodes.Get(3));
    ipv4.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer iface23 = ipv4.Assign(devices23);

    // Link N3-N0
    NS_LOG_INFO("Creating link N3-N0.");
    NetDeviceContainer devices30 = p2p.Install(nodes.Get(3), nodes.Get(0));
    ipv4.SetBase("10.1.4.0", "255.255.255.0");
    Ipv4InterfaceContainer iface30 = ipv4.Assign(devices30);

    // Enable OSPF on all nodes
    NS_LOG_INFO("Enabling OSPF on all nodes.");
    OspfHelper ospf;
    ospf.EnableOspf(nodes.Get(0));
    ospf.EnableOspf(nodes.Get(1));
    ospf.EnableOspf(nodes.Get(2));
    ospf.EnableOspf(nodes.Get(3));

    // OSPF needs time to converge, typically a few seconds
    // Let's ensure routing tables are fully populated before starting applications.
    Simulator::Schedule(Seconds(0.5), &Ipv4GlobalRoutingHelper::PopulateRoutingTables);

    // UDP Echo applications
    // Set up a UDP Echo Server on node N2 (target IP is 10.1.2.2)
    NS_LOG_INFO("Setting up UDP Echo Server on N2.");
    UdpEchoServerHelper echoServer(9); // Port 9
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(2));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // Set up a UDP Echo Client on node N0, sending to N2's 10.1.2.2 interface
    NS_LOG_INFO("Setting up UDP Echo Client on N0 to N2.");
    // N2's address on the N1-N2 link is iface12.GetAddress(1) (which is 10.1.2.2)
    UdpEchoClientHelper echoClient(iface12.GetAddress(1), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(1));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Enable PCAP tracing for all point-to-point devices
    NS_LOG_INFO("Enabling PCAP tracing.");
    p2p.EnablePcapAll("ospf-square", true); // Base filename "ospf-square", true for promiscuous mode

    // Display routing tables at the end of the simulation
    NS_LOG_INFO("Scheduling routing table display.");
    Simulator::Schedule(Seconds(10.0), &OspfHelper::PrintRoutingTableAt, &ospf, Seconds(10.0), nodes.Get(0));
    Simulator::Schedule(Seconds(10.0), &OspfHelper::PrintRoutingTableAt, &ospf, Seconds(10.0), nodes.Get(1));
    Simulator::Schedule(Seconds(10.0), &OspfHelper::PrintRoutingTableAt, &ospf, Seconds(10.0), nodes.Get(2));
    Simulator::Schedule(Seconds(10.0), &OspfHelper::PrintRoutingTableAt, &ospf, Seconds(10.0), nodes.Get(3));

    // Set simulation stop time
    Simulator::Stop(Seconds(11.0));

    // Run the simulation
    NS_LOG_INFO("Running simulation.");
    Simulator::Run();
    NS_LOG_INFO("Simulation finished.");

    // Clean up
    Simulator::Destroy();

    return 0;
}