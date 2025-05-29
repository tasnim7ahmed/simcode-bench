#include "ns3/core-module.h"
#include "ns3/network-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Create 5 nodes
    NodeContainer nodes;
    nodes.Create(5);

    // Assign names to nodes using Object Name Service
    for (uint32_t i = 0; i < nodes.GetN(); ++i) {
        std::ostringstream oss;
        oss << "Node-" << i;
        Names::Add(oss.str(), nodes.Get(i));
    }

    // Print node names and their IDs
    for (uint32_t i = 0; i < nodes.GetN(); ++i) {
        Ptr<Node> node = nodes.Get(i);
        std::string name = Names::FindName(node);
        std::cout << "Node Name: " << name << ", Node ID: " << node->GetId() << std::endl;
    }

    return 0;
}