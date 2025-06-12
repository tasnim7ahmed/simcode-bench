#include "ns3/core-module.h"
#include "ns3/network-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("NodeCreationExample");

int main(int argc, char *argv[]) {
    // Create three nodes
    NodeContainer nodes;
    nodes.Create(3);

    // Print the number of created nodes
    NS_LOG_UNCOND("Number of created nodes: " << nodes.GetN());

    return 0;
}