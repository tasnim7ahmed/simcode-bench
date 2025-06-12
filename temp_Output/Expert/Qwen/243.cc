#include "ns3/core-module.h"
#include "ns3/network-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Create three nodes
    NodeContainer nodes;
    nodes.Create(3);

    // Assign unique IDs (automatically handled by NS-3)
    for (uint32_t i = 0; i < nodes.GetN(); ++i) {
        Ptr<Node> node = nodes.Get(i);
        NS_LOG_UNCOND("Created node with ID: " << node->GetId());
    }

    // Print total number of nodes to verify creation
    NS_LOG_UNCOND("Total nodes created: " << nodes.GetN());

    // Run simulation (no events scheduled, so it just initializes and exits)
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}