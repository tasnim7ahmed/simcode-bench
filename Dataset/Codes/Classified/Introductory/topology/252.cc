#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable logging
    LogComponentEnable("TreeTopologyExample", LOG_LEVEL_INFO);

    // Define tree structure
    uint32_t numLevels = 2;  // Number of levels in the tree
    uint32_t branchingFactor = 2;  // Number of children per node
    uint32_t numNodes = 1 + branchingFactor + (branchingFactor * branchingFactor); // Root + Level 1 + Level 2

    // Create nodes
    NodeContainer nodes;
    nodes.Create(numNodes);

    // Point-to-Point Helper
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // Connect root to first level nodes
    uint32_t nodeIndex = 1;  // Start from the first child node
    for (uint32_t i = 0; i < branchingFactor; ++i) {
        NodeContainer link(nodes.Get(0), nodes.Get(nodeIndex));
        p2p.Install(link);
        NS_LOG_INFO("Connected Root Node 0 to Node " << nodeIndex);
        nodeIndex++;
    }

    // Connect first level nodes to second level nodes
    uint32_t parentIndex = 1;
    for (uint32_t i = 0; i < branchingFactor; ++i) {
        for (uint32_t j = 0; j < branchingFactor; ++j) {
            NodeContainer link(nodes.Get(parentIndex), nodes.Get(nodeIndex));
            p2p.Install(link);
            NS_LOG_INFO("Connected Node " << parentIndex << " to Node " << nodeIndex);
            nodeIndex++;
        }
        parentIndex++;
    }

    return 0;
}

