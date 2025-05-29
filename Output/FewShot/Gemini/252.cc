#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/internet-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Create nodes
    NodeContainer rootNode;
    rootNode.Create(1);

    NodeContainer intermediateNodes;
    intermediateNodes.Create(2);

    NodeContainer leafNodes;
    leafNodes.Create(4);

    // Create point-to-point helper
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    // Connect root to intermediate nodes
    NetDeviceContainer rootToInt1 = pointToPoint.Install(rootNode.Get(0), intermediateNodes.Get(0));
    NetDeviceContainer rootToInt2 = pointToPoint.Install(rootNode.Get(0), intermediateNodes.Get(1));
    std::cout << "Root connected to intermediate nodes." << std::endl;

    // Connect intermediate nodes to leaf nodes
    NetDeviceContainer int1ToLeaf1 = pointToPoint.Install(intermediateNodes.Get(0), leafNodes.Get(0));
    NetDeviceContainer int1ToLeaf2 = pointToPoint.Install(intermediateNodes.Get(0), leafNodes.Get(1));
    NetDeviceContainer int2ToLeaf3 = pointToPoint.Install(intermediateNodes.Get(1), leafNodes.Get(2));
    NetDeviceContainer int2ToLeaf4 = pointToPoint.Install(intermediateNodes.Get(1), leafNodes.Get(3));
    std::cout << "Intermediate nodes connected to leaf nodes." << std::endl;
    
    //Install Internet stack
    InternetStackHelper stack;
    stack.InstallAll();

    //Assign IP Addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer i0i1 = address.Assign(rootToInt1);
    Ipv4InterfaceContainer i0i2 = address.Assign(rootToInt2);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer i1l1 = address.Assign(int1ToLeaf1);
    Ipv4InterfaceContainer i1l2 = address.Assign(int1ToLeaf2);

    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer i2l3 = address.Assign(int2ToLeaf3);
    Ipv4InterfaceContainer i2l4 = address.Assign(int2ToLeaf4);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}