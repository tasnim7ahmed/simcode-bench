#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/object-name-service.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Create a few nodes
    NodeContainer nodes;
    nodes.Create(3);

    // Assign names to each node using Object Name Service
    ObjectNameService::SetObjectName(nodes.Get(0), "Node-One");
    ObjectNameService::SetObjectName(nodes.Get(1), "Node-Two");
    ObjectNameService::SetObjectName(nodes.Get(2), "Node-Three");

    // Print the assigned names and their unique node IDs
    for (uint32_t i = 0; i < nodes.GetN(); ++i) {
        Ptr<Node> node = nodes.Get(i);
        std::string name = ObjectNameService::GetObjectName(node);
        NS_LOG_UNCOND("Node ID: " << node->GetId() << ", Name: " << name);
    }

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}