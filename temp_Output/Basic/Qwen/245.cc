#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/object-name-service.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("NodeNamingExample");

int main(int argc, char *argv[]) {
    // Disable logging for simplicity
    LogComponentDisableAll(LOG_LEVEL_ALL);

    // Create a few nodes
    NodeContainer nodes;
    nodes.Create(3);

    // Assign names to each node using Object Name Service
    ObjectNameService::SetObjectName(nodes.Get(0), "Alice");
    ObjectNameService::SetObjectName(nodes.Get(1), "Bob");
    ObjectNameService::SetObjectName(nodes.Get(2), "Charlie");

    // Print out the assigned names and their node IDs
    for (size_t i = 0; i < nodes.GetN(); ++i) {
        Ptr<Node> node = nodes.Get(i);
        std::string name = ObjectNameService::GetObjectSimpleName(node);
        NS_LOG_UNCOND("Node ID: " << node->GetId() << ", Name: " << name);
    }

    // Clean up
    Simulator::Destroy();

    return 0;
}