#include "ns3/core-module.h"
#include "ns3/network-module.h"

using namespace ns3;

int
main(int argc, char *argv[])
{
    CommandLine cmd;
    cmd.Parse(argc, argv);

    uint32_t nNodes = 5;
    NodeContainer nodes;
    nodes.Create(nNodes);

    // Assign names using the Object Name Service
    for (uint32_t i = 0; i < nNodes; ++i)
    {
        std::ostringstream oss;
        oss << "Node-" << i;
        Names::Add(oss.str(), nodes.Get(i));
    }

    // Print assigned names and node IDs
    for (uint32_t i = 0; i < nNodes; ++i)
    {
        Ptr<Node> node = nodes.Get(i);
        std::string name = Names::FindName(node);
        std::cout << "Node ID: " << node->GetId() << ", Name: " << name << std::endl;
    }

    Simulator::Destroy();
    return 0;
}