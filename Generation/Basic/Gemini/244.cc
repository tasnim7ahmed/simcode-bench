#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include <iostream>

int main(int argc, char *argv[])
{
    // 1. Create multiple nodes
    uint32_t numNodes = 5;
    ns3::NodeContainer nodes;
    nodes.Create(numNodes);

    // 2. Iterate over the nodes and print their assigned unique node IDs
    std::cout << "Node IDs assigned after creation:" << std::endl;
    for (uint32_t i = 0; i < nodes.GetN(); ++i)
    {
        ns3::Ptr<ns3::Node> node = nodes.Get(i);
        std::cout << "  Node at index " << i << " has ns-3 Node ID: " << node->GetId() << std::endl;
    }

    // Clean up ns-3 resources. Required even if Simulator::Run() is not called.
    ns3::Simulator::Destroy();

    return 0;
}