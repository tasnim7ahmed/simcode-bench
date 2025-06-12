#include "ns3/core-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Create 5 nodes as an example
    uint32_t numNodes = 5;
    NodeContainer nodes;
    nodes.Create(numNodes);

    for (uint32_t i = 0; i < nodes.GetN(); ++i) {
        Ptr<Node> node = nodes.Get(i);
        std::cout << "Node " << i << " has NodeId: " << node->GetId() << std::endl;
    }

    return 0;
}