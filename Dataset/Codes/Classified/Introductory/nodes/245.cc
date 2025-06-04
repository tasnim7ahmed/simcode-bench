#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/object.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable logging
    LogComponentEnable("NodeNamingExample", LOG_LEVEL_INFO);

    // Create a NodeContainer and generate 3 nodes
    NodeContainer nodes;
    nodes.Create(3);

    // Assign names to each node using Object Name Service (ONS)
    for (uint32_t i = 0; i < nodes.GetN(); ++i) {
        std::ostringstream nodeName;
        nodeName << "Node-" << i;
        Names::Add(nodeName.str(), nodes.Get(i));

        // Print assigned names and node IDs
        NS_LOG_INFO(nodeName.str() << " has ID: " << nodes.Get(i)->GetId());
    }

    return 0;
}

