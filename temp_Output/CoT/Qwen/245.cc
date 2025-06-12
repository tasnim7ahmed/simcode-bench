#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/object-name-service.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("NodeNamingExample");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    // Create 5 nodes
    NodeContainer nodes;
    nodes.Create(5);

    // Assign names to each node using Object Name Service
    for (uint32_t i = 0; i < nodes.GetN(); ++i) {
        std::string name = "Node_" + std::to_string(i);
        Names::Add(name, nodes.Get(i));
    }

    // Print the assigned names along with their unique node IDs
    for (uint32_t i = 0; i < nodes.GetN(); ++i) {
        Ptr<Node> node = nodes.Get(i);
        std::string name = Names::FindName(node);
        std::cout << "Node ID: " << node->GetId() << ", Assigned Name: " << name << std::endl;
    }

    return 0;
}