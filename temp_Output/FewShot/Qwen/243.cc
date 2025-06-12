#include "ns3/core-module.h"
#include "ns3/network-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Create a container for three nodes
    NodeContainer nodes;
    nodes.Create(3);

    // Print the number of created nodes to verify
    std::cout << "Number of nodes created: " << nodes.GetN() << std::endl;

    return 0;
}