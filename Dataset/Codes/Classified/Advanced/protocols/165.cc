#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/ipv4-list-routing-helper.h"
#include "ns3/ospf-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("OspfRoutingExample");

int main(int argc, char *argv[])
{
    // Enable logging for OSPF and other components
    LogComponentEnable("OspfRoutingExample", LOG_LEVEL_INFO);

    // Create 4 nodes
    NodeContainer nodes;
    nodes.Create(4);

    // Create Point-to-Point links between the nodes to form a square topology
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // Install links: Node 0-1, Node 1-2, Node 2-3, Node 3-0 (square)
    NetDeviceContainer devices01 = p2p.Install(nodes.Get(0), nodes.Get(1));
    NetDeviceContainer devices12 = p2p.Install(nodes.Get(1), nodes.Get(2));
    NetDeviceContainer devices23 = p2p.Install(nodes.Get(2), nodes.Get(3));
    NetDeviceContainer devices30 = p2p.Install(nodes.Get(3), nodes.Get(0));

    // Install Internet stack with OSPF routing on all nodes
    InternetStackHelper internet;
    
    // Enable OSPF routing
    Ipv4ListRoutingHelper listRouting;
    OspfHelper ospfRouting;
    listRouting.Add(ospfRouting, 0);
    internet.SetRoutingHelper(listRouting);  // Use OSPF as the routing protocol
    internet.Install(nodes);

    // Assign IP addresses to the network interfaces
    Ipv4AddressHelper ipv4;
    
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer ifaces01 = ipv4.Assign(devices01);  // Link 0-1

    ipv4.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer ifaces12 = ipv4.Assign(devices12);  // Link 1-2

    ipv4.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer ifaces23 = ipv4.Assign(devices23);  // Link 2-3

    ipv4.SetBase("10.1.4.0", "255.255.255.0");
    Ipv4InterfaceContainer ifaces30 = ipv4.Assign(devices30);  // Link 3-0

    // Set up a simple application: Echo server on node 3
    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(3));  // Server on node 3
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // Set up a client on node 0 to send packets to node 3
    UdpEchoClientHelper echoClient(ifaces23.GetAddress(1), 9);  // Node 0 sends to node 3
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));  // Client on node 0
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Enable routing table printing on all nodes for OSPF
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();
    Simulator::Schedule(Seconds(2.0), &Ipv4GlobalRoutingHelper::PrintRoutingTableAllAt, Seconds(2.0), Create<OutputStreamWrapper>(&std::cout));

    // Enable pcap tracing to observe packet transmission
    p2p.EnablePcapAll("ospf-routing");

    // Run the simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}

