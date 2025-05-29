#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include <iostream>

using namespace ns3;

int main() {
    // Enable logging (optional)
    LogComponentEnable("ObjectNames", LOG_LEVEL_INFO);

    // Create nodes
    NodeContainer nodes;
    nodes.Create(3);

    // Assign names to the nodes
    Names::Add("NodeZero", nodes.Get(0));
    Names::Add("NodeOne", nodes.Get(1));
    Names::Add("NodeTwo", nodes.Get(2));

    // Print the names and node IDs
    for (uint32_t i = 0; i < nodes.GetN(); ++i) {
        Ptr<Node> node = nodes.Get(i);
        std::cout << "Node Name: " << Names::FindName(node)
                  << ", Node ID: " << node->GetId() << std::endl;
    }

    Simulator::Run();
    Simulator::Destroy();
    return 0;
}