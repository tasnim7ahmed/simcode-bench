#include "ns3/core-module.h"
#include "ns3/network-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("NodeNamingExample");

int
main(int argc, char* argv[])
{
    // Create a few nodes
    NodeContainer nodes;
    nodes.Create(5);

    // Assign names to each node using the Object Name Service
    Names::Add("/Names/Node0", *nodes.Get(0));
    Names::Add("/Names/Node1", *nodes.Get(1));
    Names::Add("/Names/Node2", *nodes.Get(2));
    Names::Add("/Names/Node3", *nodes.Get(3));
    Names::Add("/Names/Node4", *nodes.Get(4));

    // Print out the assigned names and their corresponding node IDs
    for (uint32_t i = 0; i < nodes.GetN(); ++i)
    {
        Ptr<Node> node = nodes.Get(i);
        std::string name = Names::FindName(node);
        NS_LOG_UNCOND("Node ID: " << node->GetId() << ", Name: " << name);
    }

    Simulator::Destroy();
    return 0;
}