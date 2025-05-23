#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

// Do not use 'using namespace ns3;' in global scope as per ns-3 coding style.

int main(int argc, char *argv[])
{
    // Enable logging for UdpEchoClient and UdpEchoServer applications
    ns3::LogComponentEnable("UdpEchoClientApplication", ns3::LOG_LEVEL_INFO);
    ns3::LogComponentEnable("UdpEchoServerApplication", ns3::LOG_LEVEL_INFO);
    ns3::LogComponentEnable("BranchTopologySimulation", ns3::LOG_LEVEL_INFO);

    // Create three nodes: Node 0 (central), Node 1, Node 2
    ns3::NodeContainer nodes;
    nodes.Create(3); // nodes.Get(0), nodes.Get(1), nodes.Get(2)

    // Get individual nodes for clarity
    ns3::Ptr<ns3::Node> n0 = nodes.Get(0); // Central node
    ns3::Ptr<ns3::Node> n1 = nodes.Get(1); // Branch node 1
    ns3::Ptr<ns3::Node> n2 = nodes.Get(2); // Branch node 2

    // Create the PointToPointHelper and set attributes for the links
    ns3::PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", ns3::StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", ns3::StringValue("5ms"));

    // Install Internet stack on all nodes
    ns3::InternetStackHelper stack;
    stack.Install(nodes);

    // Connect Node 0 to Node 1 via a point-to-point link
    ns3::NetDeviceContainer devices01;
    devices01 = p2p.Install(n0, n1);

    // Connect Node 0 to Node 2 via another point-to-point link
    ns3::NetDeviceContainer devices02;
    devices02 = p2p.Install(n0, n2);

    // Assign IP addresses to the link between Node 0 and Node 1 (10.1.1.0/24)
    ns3::Ipv4AddressHelper ipv4_01;
    ipv4_01.SetBase("10.1.1.0", "255.255.255.0");
    ns3::Ipv4InterfaceContainer interfaces01 = ipv4_01.Assign(devices01); // n0 gets 10.1.1.1, n1 gets 10.1.1.2

    // Assign IP addresses to the link between Node 0 and Node 2 (10.1.2.0/24)
    ns3::Ipv4AddressHelper ipv4_02;
    ipv4_02.SetBase("10.1.2.0", "255.255.255.0");
    ns3::Ipv4InterfaceContainer interfaces02 = ipv4_02.Assign(devices02); // n0 gets 10.1.2.1, n2 gets 10.1.2.2

    // Populate routing tables for all nodes. This is crucial for multi-homed nodes.
    ns3::Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Setup UDP Echo Servers
    // Server on Node 1 (listening on 10.1.1.2, port 9)
    ns3::UdpEchoServerHelper echoServer1(9); // Port 9 is the standard echo port
    ns3::ApplicationContainer serverApps1 = echoServer1.Install(n1);
    serverApps1.Start(ns3::Seconds(1.0)); // Server starts at 1 second
    serverApps1.Stop(ns3::Seconds(9.5));  // Server stops at 9.5 seconds

    // Server on Node 2 (listening on 10.1.2.2, port 9)
    ns3::UdpEchoServerHelper echoServer2(9);
    ns3::ApplicationContainer serverApps2 = echoServer2.Install(n2);
    serverApps2.Start(ns3::Seconds(1.0)); // Server starts at 1 second
    serverApps2.Stop(ns3::Seconds(9.5));  // Server stops at 9.5 seconds

    // Setup UDP Echo Clients on Node 0
    // Client on Node 0 targeting Node 1's server (10.1.1.2)
    ns3::UdpEchoClientHelper echoClient1(interfaces01.GetAddress(1), 9); // interfaces01.GetAddress(1) is 10.1.1.2
    echoClient1.SetAttribute("MaxPackets", ns3::UintegerValue(3)); // Send 3 packets
    echoClient1.SetAttribute("Interval", ns3::TimeValue(ns3::Seconds(1.0))); // 1 packet per second
    echoClient1.SetAttribute("PacketSize", ns3::UintegerValue(512)); // 512 bytes per packet
    ns3::ApplicationContainer clientApps1 = echoClient1.Install(n0);
    clientApps1.Start(ns3::Seconds(2.0)); // Client starts sending at 2 seconds
    clientApps1.Stop(ns3::Seconds(8.0));  // Client stops sending at 8 seconds

    // Client on Node 0 targeting Node 2's server (10.1.2.2)
    ns3::UdpEchoClientHelper echoClient2(interfaces02.GetAddress(1), 9); // interfaces02.GetAddress(1) is 10.1.2.2
    echoClient2.SetAttribute("MaxPackets", ns3::UintegerValue(3)); // Send 3 packets
    echoClient2.SetAttribute("Interval", ns3::TimeValue(ns3::Seconds(1.0))); // 1 packet per second
    echoClient2.SetAttribute("PacketSize", ns3::UintegerValue(512)); // 512 bytes per packet
    ns3::ApplicationContainer clientApps2 = echoClient2.Install(n0);
    clientApps2.Start(ns3::Seconds(2.5)); // Client starts sending at 2.5 seconds (slightly offset)
    clientApps2.Stop(ns3::Seconds(8.5));  // Client stops sending at 8.5 seconds

    // Enable PCAP tracing for traffic analysis
    // Tracing for link 0-1 (Node 0 and Node 1)
    p2p.EnablePcap("branch-topology-link01", devices01);
    // Tracing for link 0-2 (Node 0 and Node 2)
    p2p.EnablePcap("branch-topology-link02", devices02);

    // Set simulation duration
    ns3::Simulator::Stop(ns3::Seconds(10.0));

    // Run the simulation
    ns3::Simulator::Run();

    // Clean up simulation resources
    ns3::Simulator::Destroy();

    return 0;
}