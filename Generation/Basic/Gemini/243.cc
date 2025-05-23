#include "ns3/core-module.h"
#include "ns3/node-container.h"

int
main(int argc, char* argv[])
{
    using namespace ns3;

    NodeContainer nodes;
    nodes.Create(3); // Create three nodes

    std::cout << "Number of nodes created: " << nodes.GetN() << std::endl;

    // Node IDs are automatically assigned starting from 0.
    // For example, to verify IDs (optional, not strictly required by prompt but useful for understanding):
    // for (uint32_t i = 0; i < nodes.GetN(); ++i)
    // {
    //     Ptr<Node> node = nodes.Get(i);
    //     std::cout << "Node ID: " << node->GetId() << std::endl;
    // }

    // No simulation events are scheduled, so Simulator::Run() is not strictly needed.
    // However, Simulator::Destroy() is good practice for cleanup of singletons.
    Simulator::Destroy();

    return 0;
}