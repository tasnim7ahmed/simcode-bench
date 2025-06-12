#include "ns3/core-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("NodeIdsExample");

int
main(int argc, char* argv[])
{
    // Create a specified number of nodes
    uint32_t numNodes = 5;
    NodeContainer nodes;
    nodes.Create(numNodes);

    // Iterate over the nodes and print their assigned node IDs
    for (uint32_t i = 0; i < numNodes; ++i)
    {
        Ptr<Node> node = nodes.Get(i);
        NS_LOG_UNCOND("Node ID for node index " << i << ": " << node->GetId());
    }

    // Simulation cleanup
    Simulator::Destroy();

    return 0;
}