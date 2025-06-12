#include "ns3/core-module.h"
#include "ns3/network-module.h"

using namespace ns3;

int 
main(int argc, char *argv[])
{
    // Enable logging for this script (optional)
    // LogComponentEnable("CreateNodesExample", LOG_LEVEL_INFO);

    // Create three nodes
    NodeContainer nodes;
    nodes.Create(3);

    // Print their IDs
    for (uint32_t i = 0; i < nodes.GetN(); ++i)
    {
        Ptr<Node> node = nodes.Get(i);
        std::cout << "Node " << i << " has ID: " << node->GetId() << std::endl;
    }

    // Print total node count (should be 3)
    std::cout << "Total number of nodes created: " << nodes.GetN() << std::endl;

    return 0;
}