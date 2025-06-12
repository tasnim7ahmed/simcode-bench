#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/node-container.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    NodeContainer nodes;
    nodes.Create(5); // Create 5 nodes

    for (auto node = nodes.Begin(); node != nodes.End(); ++node) {
        Ptr<Node> currentNode = *node;
        std::cout << "Node ID: " << currentNode->GetId() << std::endl;
    }

    Simulator::Destroy();
    return 0;
}