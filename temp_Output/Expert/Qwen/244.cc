#include "ns3/core-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    NodeContainer nodes;
    nodes.Create(5); // Create 5 nodes

    for (uint32_t i = 0; i < nodes.GetN(); ++i) {
        Ptr<Node> node = nodes.Get(i);
        NS_LOG_UNCOND("Node ID: " << node->GetId());
    }

    Simulator::Destroy();
    return 0;
}