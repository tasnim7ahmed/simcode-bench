#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ipv4-address-helper.h"
#include <iostream>

using namespace ns3;

int main(int argc, char *argv[])
{
    // Create three nodes
    NodeContainer nodes;
    nodes.Create(3); // Node 0, Node 1, Node 2

    // Configure Point-to-Point link properties
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    // Create Link 1: Node 0 <---> Node 1
    NetDeviceContainer devices01 = pointToPoint.Install(nodes.Get(0), nodes.Get(1));

    // Create Link 2: Node 1 <---> Node 2
    NetDeviceContainer devices12 = pointToPoint.Install(nodes.Get(1), nodes.Get(2));

    // Install Internet stack on all nodes
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses for Link 1 (Node 0 - Node 1)
    Ipv4AddressHelper address01;
    address01.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces01 = address01.Assign(devices01);

    // Assign IP addresses for Link 2 (Node 1 - Node 2)
    Ipv4AddressHelper address12;
    address12.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces12 = address12.Assign(devices12);

    // Print confirmation of the links established
    std::cout << "Network Topology (3 nodes, 2 P2P links):" << std::endl;

    std::cout << "Link 1: Node " << nodes.Get(0)->GetId() << " <---> Node " << nodes.Get(1)->GetId() << std::endl;
    std::cout << "  Node " << nodes.Get(0)->GetId() << " IP Address: " << interfaces01.GetAddress(0) << std::endl;
    std::cout << "  Node " << nodes.Get(1)->GetId() << " IP Address (for Link 1): " << interfaces01.GetAddress(1) << std::endl;
    std::cout << "  Subnet: " << address01.GetBaseNetwork() << std::endl;

    std::cout << std::endl;

    std::cout << "Link 2: Node " << nodes.Get(1)->GetId() << " <---> Node " << nodes.Get(2)->GetId() << std::endl;
    std::cout << "  Node " << nodes.Get(1)->GetId() << " IP Address (for Link 2): " << interfaces12.GetAddress(0) << std::endl;
    std::cout << "  Node " << nodes.Get(2)->GetId() << " IP Address: " << interfaces12.GetAddress(1) << std::endl;
    std::cout << "  Subnet: " << address12.GetBaseNetwork() << std::endl;

    return 0;
}