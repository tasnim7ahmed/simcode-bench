#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("RingTopologySimulation");

int main(int argc, char *argv[])
{
    // Enable logging for relevant components
    LogComponentEnable("RingTopologySimulation", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
    LogComponentEnable("PointToPointNetDevice", LOG_LEVEL_DEBUG);
    LogComponentEnable("Ipv4L3Protocol", LOG_LEVEL_DEBUG);
    LogComponentEnable("Ipv4GlobalRouting", LOG_LEVEL_DEBUG);
    LogComponentEnable("ArpCache", LOG_LEVEL_DEBUG);

    // Create 3 nodes
    NodeContainer nodes;
    nodes.Create(3); // nodes.Get(0), nodes.Get(1), nodes.Get(2)

    // Configure Point-to-Point link attributes
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices01, devices12, devices20;

    // Connect Node 0 to Node 1
    devices01 = p2p.Install(nodes.Get(0), nodes.Get(1));
    // Connect Node 1 to Node 2
    devices12 = p2p.Install(nodes.Get(1), nodes.Get(2));
    // Connect Node 2 to Node 0 (completing the ring)
    devices20 = p2p.Install(nodes.Get(2), nodes.Get(0));

    // Install Internet stack on all nodes
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses to each link
    Ipv4AddressHelper address;

    // Link 0-1: 10.1.1.0/24
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces01 = address.Assign(devices01); 
    // Node 0: 10.1.1.1, Node 1: 10.1.1.2

    // Link 1-2: 10.1.2.0/24
    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces12 = address.Assign(devices12); 
    // Node 1: 10.1.2.1, Node 2: 10.1.2.2

    // Link 2-0: 10.1.3.0/24
    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces20 = address.Assign(devices20); 
    // Node 2: 10.1.3.1, Node 0: 10.1.3.2

    // Populate global routing tables
    // This allows nodes to communicate with non-adjacent nodes in the ring.
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Setup applications for communication across the ring

    // Scenario 1: Node 0 (client) sends to Node 2 (server)
    // Path: Node 0 -> Node 1 -> Node 2
    UdpEchoServerHelper echoServer1(9); // Server listens on port 9
    ApplicationContainer serverApps1 = echoServer1.Install(nodes.Get(2)); // Install on Node 2
    serverApps1.Start(Seconds(1.0));
    serverApps1.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient1(interfaces12.GetAddress(1), 9); // Send to Node 2's IP on Link 1-2 (10.1.2.2)
    echoClient1.SetAttribute("MaxPackets", UintegerValue(1));
    echoClient1.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient1.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps1 = echoClient1.Install(nodes.Get(0)); // Install on Node 0
    clientApps1.Start(Seconds(2.0)); // Start after server
    clientApps1.Stop(Seconds(10.0));

    // Scenario 2: Node 2 (client) sends to Node 0 (server)
    // Path: Node 2 -> Node 1 -> Node 0
    UdpEchoServerHelper echoServer2(10); // Server listens on port 10
    ApplicationContainer serverApps2 = echoServer2.Install(nodes.Get(0)); // Install on Node 0
    serverApps2.Start(Seconds(1.0));
    serverApps2.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient2(interfaces01.GetAddress(0), 10); // Send to Node 0's IP on Link 0-1 (10.1.1.1)
    echoClient2.SetAttribute("MaxPackets", UintegerValue(1));
    echoClient2.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient2.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps2 = echoClient2.Install(nodes.Get(2)); // Install on Node 2
    clientApps2.Start(Seconds(3.0)); // Stagger start time
    clientApps2.Stop(Seconds(10.0));

    // Set simulation duration
    Simulator::Stop(Seconds(10.0));

    // Run the simulation
    Simulator::Run();

    // Clean up simulation resources
    Simulator::Destroy();

    return 0;
}