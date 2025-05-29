#include "ns3/core-module.h"
#include "ns3/network-module.h"

using namespace ns3;

int
main(int argc, char *argv[])
{
    // Set up logging
    LogComponentEnable("NodeNamingExample", LOG_LEVEL_INFO);

    // Number of nodes
    uint32_t numNodes = 5;

    // Create nodes
    NodeContainer nodes;
    nodes.Create(numNodes);

    // Assign individual names using Object Name Service (ONS)
    for (uint32_t i = 0; i < nodes.GetN(); ++i)
    {
        std::ostringstream oss;
        oss << "Node-" << i;
        Names::Add(oss.str(), nodes.Get(i));
    }

    // Print assigned names and their unique node IDs
    NS_LOG_UNCOND("Assigned Names and Node IDs:");
    for (uint32_t i = 0; i < nodes.GetN(); ++i)
    {
        Ptr<Node> node = nodes.Get(i);
        std::string nodeName = Names::FindName(node);
        uint32_t nodeId = node->GetId();
        std::cout << "Name: " << nodeName << ", NodeId: " << nodeId << std::endl;
    }

    Simulator::Run();
    Simulator::Destroy();
    return 0;
}