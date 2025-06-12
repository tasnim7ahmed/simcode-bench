#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include <iostream>

using namespace ns3;

int main(int argc, char *argv[]) {
    // Create 5 nodes
    NodeContainer nodes;
    nodes.Create(5);

    // Iterate over the nodes and print their IDs
    for (uint32_t i = 0; i < nodes.GetN(); ++i) {
        Ptr<Node> node = nodes.Get(i);
        std::cout << "Node " << i << " has Node ID: " << node->GetId() << std::endl;
    }

    return 0;
}