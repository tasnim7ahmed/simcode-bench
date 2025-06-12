#include "ns3/core-module.h"
#include "ns3/network-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Create a node container with multiple nodes
    NodeContainer nodes;
    nodes.Create(5);  // Create 5 nodes

    // Iterate through the nodes and print their assigned node IDs
    for (auto node = nodes.Begin(); node != nodes.End(); ++node) {
        uint32_t nodeId = (*node)->GetId();
        NS_LOG_UNCOND("Node ID: " << nodeId);
    }

    return 0;
}