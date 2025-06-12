#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"

using namespace ns3;

int main (int argc, char *argv[])
{
    // Enable logging (optional for debugging)
    // LogComponentEnable("PointToPointNetDevice", LOG_LEVEL_INFO);

    // Simulation parameters for the tree
    uint32_t numIntermediateNodes = 2;          // Number of intermediate nodes directly connected to root
    uint32_t numLeafNodesPerIntermediate = 2;   // Number of leaves per intermediate node

    // Calculate total nodes
    uint32_t numLeafNodes = numIntermediateNodes * numLeafNodesPerIntermediate;
    uint32_t totalNodes = 1 + numIntermediateNodes + numLeafNodes; // root + intermediates + leaves

    // Create all nodes
    NodeContainer allNodes;
    allNodes.Create(totalNodes);

    // Node index mapping
    Ptr<Node> rootNode = allNodes.Get(0);

    // Store intermediate node pointers and their indices
    std::vector< Ptr<Node> > intermediateNodes;
    std::vector< uint32_t > intermediateIndices;

    for (uint32_t i = 0; i < numIntermediateNodes; ++i)
    {
        Ptr<Node> node = allNodes.Get(1 + i);
        intermediateNodes.push_back(node);
        intermediateIndices.push_back(1 + i);
    }

    // Store leaf node pointers and their indices
    std::vector< Ptr<Node> > leafNodes;
    std::vector< uint32_t > leafIndices;

    for (uint32_t i = 0; i < numLeafNodes; ++i)
    {
        Ptr<Node> node = allNodes.Get(1 + numIntermediateNodes + i);
        leafNodes.push_back(node);
        leafIndices.push_back(1 + numIntermediateNodes + i);
    }

    // Point-to-Point helper
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // Connect root node to intermediate nodes
    for (uint32_t i = 0; i < numIntermediateNodes; ++i)
    {
        NodeContainer pair(rootNode, intermediateNodes[i]);
        NetDeviceContainer devices = p2p.Install(pair);

        std::cout << "Connected Root (node 0) <--> Intermediate (node " << intermediateIndices[i] << ")" << std::endl;
    }

    // Connect each intermediate node to its leaf nodes
    uint32_t currentLeafIndex = 0;
    for (uint32_t i = 0; i < numIntermediateNodes; ++i)
    {
        for (uint32_t j = 0; j < numLeafNodesPerIntermediate; ++j)
        {
            uint32_t leafIdx = currentLeafIndex;
            NodeContainer pair(intermediateNodes[i], leafNodes[leafIdx]);
            NetDeviceContainer devices = p2p.Install(pair);

            std::cout << "Connected Intermediate (node " << intermediateIndices[i] << ") <--> Leaf (node " << leafIndices[leafIdx] << ")" << std::endl;
            currentLeafIndex++;
        }
    }

    Simulator::Stop (Seconds(1.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}