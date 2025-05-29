#include "ns3/core-module.h"
#include "ns3/network-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    CommandLine cmd;
    cmd.Parse(argc, argv);

    uint32_t numNodes = 5;

    NodeContainer nodes;
    nodes.Create(numNodes);

    Ptr<Object> obj;
    std::ostringstream oss;

    // Assign names to nodes using Object Name Service
    for (uint32_t i = 0; i < numNodes; ++i)
    {
        oss.str("");
        oss << "Node-" << i;
        Names::Add(oss.str(), nodes.Get(i));
    }

    // Print node names and IDs
    std::cout << "Assigned node names and IDs:" << std::endl;
    for (uint32_t i = 0; i < numNodes; ++i)
    {
        Ptr<Node> node = nodes.Get(i);
        std::string name = Names::FindName(node);
        std::cout << "Node ID: " << node->GetId() << ", Name: " << name << std::endl;
    }

    return 0;
}