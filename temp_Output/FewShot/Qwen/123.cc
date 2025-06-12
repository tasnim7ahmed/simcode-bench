#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-routing-table-entry.h"
#include "ns3/ipv4-list-routing-helper.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ospf-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable logging for debugging
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create four nodes (forming a square: A-B-C-D-A)
    NodeContainer nodes;
    nodes.Create(4);

    // Connect the nodes in a square topology using point-to-point links
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer linkAB = p2p.Install(nodes.Get(0), nodes.Get(1));
    NetDeviceContainer linkBC = p2p.Install(nodes.Get(1), nodes.Get(2));
    NetDeviceContainer linkCD = p2p.Install(nodes.Get(2), nodes.Get(3));
    NetDeviceContainer linkDA = p2p.Install(nodes.Get(3), nodes.Get(0));

    // Install Internet stack with OSPF routing
    OspfHelper ospfRouting;
    InternetStackHelper stack;
    stack.SetRoutingHelper(ospfRouting); // Use OSPF as the routing protocol
    stack.Install(nodes);

    // Assign IP addresses to each link
    Ipv4AddressHelper address;

    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfacesAB = address.Assign(linkAB);
    address.SetBase("192.168.2.0", "255.255.255.0");
    Ipv4InterfaceContainer interfacesBC = address.Assign(linkBC);
    address.SetBase("192.168.3.0", "255.255.255.0");
    Ipv4InterfaceContainer interfacesCD = address.Assign(linkCD);
    address.SetBase("192.168.4.0", "255.255.255.0");
    Ipv4InterfaceContainer interfacesDA = address.Assign(linkDA);

    // Enable pcap tracing on all point-to-point links
    p2p.EnablePcapAll("ospf-square");

    // Set up UDP Echo Server on node 2 (destination node)
    uint16_t port = 9;  // UDP port number
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApp = echoServer.Install(nodes.Get(2));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Set up UDP Echo Client on node 0 (source node)
    UdpEchoClientHelper echoClient(interfacesBC.GetAddress(1), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp = echoClient.Install(nodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Print routing tables at the end of simulation
    Simulator::Schedule(Seconds(10.0), &Ipv4RoutingHelper::PrintRoutingTableAllAt,
                        Seconds(10.0), Ipv4RoutingTableEntry());

    // Run the simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}