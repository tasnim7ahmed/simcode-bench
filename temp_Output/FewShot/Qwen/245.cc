#include "ns3/core-module.h"
#include "ns3/network-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Create a set of nodes
    NodeContainer nodes;
    nodes.Create(3);  // Creating 3 nodes

    // Assign names to each node using Object Name Service
    Names::Add("Node0", nodes.Get(0));
    Names::Add("Node1", nodes.Get(1));
    Names::Add("Node2", nodes.Get(2));

    // Print the assigned names and their corresponding node IDs
    for (uint32_t i = 0; i < nodes.GetN(); ++i) {
        Ptr<Node> node = nodes.Get(i);
        std::string name = Names::FindName(node);
        NS_LOG_UNCOND("Node ID: " << node->GetId() << ", Assigned Name: " << name);
    }

    return 0;
}