#include "ns3/core-module.h"
#include "ns3/network-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Create a NodeContainer to hold three nodes
    NodeContainer nodes;
    nodes.Create(3);

    // Print the number of created nodes
    std::cout << "Number of nodes created: " << nodes.GetN() << std::endl;

    return 0;
}