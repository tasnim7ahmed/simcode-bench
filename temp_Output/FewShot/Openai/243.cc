#include "ns3/core-module.h"
#include "ns3/network-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // Create three nodes
    NodeContainer nodes;
    nodes.Create(3);

    // Print the number of nodes and their IDs
    std::cout << "Number of nodes: " << nodes.GetN() << std::endl;
    for (uint32_t i = 0; i < nodes.GetN(); ++i)
    {
        std::cout << "Node " << i << " ID: " << nodes.Get(i)->GetId() << std::endl;
    }

    return 0;
}