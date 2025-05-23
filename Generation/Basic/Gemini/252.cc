#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/names-helper.h"
#include <iostream>

int main(int argc, char *argv[])
{
    // 1. Create Nodes
    NodeContainer rootNodeContainer;
    rootNodeContainer.Create(1);
    Ptr<Node> root = rootNodeContainer.Get(0);
    Names::Add("RootNode", root);

    NodeContainer intermediateNodes;
    intermediateNodes.Create(2);
    Names::Add("IntermediateNode_0", intermediateNodes.Get(0));
    Names::Add("IntermediateNode_1", intermediateNodes.Get(1));

    NodeContainer leafNodes;
    leafNodes.Create(4);
    Names::Add("LeafNode_0", leafNodes.Get(0));
    Names::Add("LeafNode_1", leafNodes.Get(1));
    Names::Add("LeafNode_2", leafNodes.Get(2));
    Names::Add("LeafNode_3", leafNodes.Get(3));

    // Consolidate all nodes for InternetStack installation
    NodeContainer allNodes;
    allNodes.Add(rootNodeContainer);
    allNodes.Add(intermediateNodes);
    allNodes.Add(leafNodes);

    // 2. Install Internet Stack on all nodes
    InternetStackHelper internet;
    internet.Install(allNodes);

    // 3. Configure Point-to-Point links
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // 4. Assign IP Addresses and establish connections
    Ipv4AddressHelper ipv4;
    Ipv4InterfaceContainer interfaces[6]; // To store interfaces for each link

    std::cout << "Tree Topology Connection Details:" << std::endl;
    std::cout << "---------------------------------" << std::endl;

    // Root to IntermediateNode_0
    NetDeviceContainer devices1 = p2p.Install(root, intermediateNodes.Get(0));
    ipv4.SetBase("10.0.0.0", "255.255.255.252");
    interfaces[0] = ipv4.Assign(devices1);
    std::cout << "Link: " << Names::FindName(root) << " <-> " << Names::FindName(intermediateNodes.Get(0))
              << " | IPs: " << interfaces[0].GetAddress(0) << " / " << interfaces[0].GetAddress(1) << std::endl;

    // Root to IntermediateNode_1
    NetDeviceContainer devices2 = p2p.Install(root, intermediateNodes.Get(1));
    ipv4.SetBase("10.0.0.4", "255.255.255.252");
    interfaces[1] = ipv4.Assign(devices2);
    std::cout << "Link: " << Names::FindName(root) << " <-> " << Names::FindName(intermediateNodes.Get(1))
              << " | IPs: " << interfaces[1].GetAddress(0) << " / " << interfaces[1].GetAddress(1) << std::endl;

    // IntermediateNode_0 to LeafNode_0
    NetDeviceContainer devices3 = p2p.Install(intermediateNodes.Get(0), leafNodes.Get(0));
    ipv4.SetBase("10.0.0.8", "255.255.255.252");
    interfaces[2] = ipv4.Assign(devices3);
    std::cout << "Link: " << Names::FindName(intermediateNodes.Get(0)) << " <-> " << Names::FindName(leafNodes.Get(0))
              << " | IPs: " << interfaces[2].GetAddress(0) << " / " << interfaces[2].GetAddress(1) << std::endl;

    // IntermediateNode_0 to LeafNode_1
    NetDeviceContainer devices4 = p2p.Install(intermediateNodes.Get(0), leafNodes.Get(1));
    ipv4.SetBase("10.0.0.12", "255.255.255.252");
    interfaces[3] = ipv4.Assign(devices4);
    std::cout << "Link: " << Names::FindName(intermediateNodes.Get(0)) << " <-> " << Names::FindName(leafNodes.Get(1))
              << " | IPs: " << interfaces[3].GetAddress(0) << " / " << interfaces[3].GetAddress(1) << std::endl;

    // IntermediateNode_1 to LeafNode_2
    NetDeviceContainer devices5 = p2p.Install(intermediateNodes.Get(1), leafNodes.Get(2));
    ipv4.SetBase("10.0.0.16", "255.255.255.252");
    interfaces[4] = ipv4.Assign(devices5);
    std::cout << "Link: " << Names::FindName(intermediateNodes.Get(1)) << " <-> " << Names::FindName(leafNodes.Get(2))
              << " | IPs: " << interfaces[4].GetAddress(0) << " / " << interfaces[4].GetAddress(1) << std::endl;

    // IntermediateNode_1 to LeafNode_3
    NetDeviceContainer devices6 = p2p.Install(intermediateNodes.Get(1), leafNodes.Get(3));
    ipv4.SetBase("10.0.0.20", "255.255.255.252");
    interfaces[5] = ipv4.Assign(devices6);
    std::cout << "Link: " << Names::FindName(intermediateNodes.Get(1)) << " <-> " << Names::FindName(leafNodes.Get(3))
              << " | IPs: " << interfaces[5].GetAddress(0) << " / " << interfaces[5].GetAddress(1) << std::endl;

    std::cout << "---------------------------------" << std::endl;

    // 5. Populate routing tables (essential for multi-hop communication)
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // 6. Run Simulation (even if no applications, this initializes the network devices and routing)
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}