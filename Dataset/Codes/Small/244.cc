#include "ns3/core-module.h"
#include "ns3/network-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable logging
    LogComponentEnable("NodeIDExample", LOG_LEVEL_INFO);

    // Create a NodeContainer and generate 5 nodes
    NodeContainer nodes;
    nodes.Create(5);

    // Iterate through the nodes and print their IDs
    for (uint32_t i = 0; i < nodes.GetN(); ++i) {
        Ptr<Node> node = nodes.Get(i);
        NS_LOG_INFO("Node " << i << " has ID: " << node->GetId());
    }

    return 0;
}

