#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include <iostream>

using namespace ns3;

int main(int argc, char *argv[]) {
    // Create three nodes
    NodeContainer nodes;
    nodes.Create(3);

    // Verify the number of nodes created
    std::cout << "Number of nodes created: " << nodes.GetN() << std::endl;

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}