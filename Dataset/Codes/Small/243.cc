#include "ns3/core-module.h"
#include "ns3/network-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable logging
    LogComponentEnable("NodeCreationExample", LOG_LEVEL_INFO);

    // Create a container to hold nodes
    NodeContainer nodes;
    
    // Create 3 nodes
    nodes.Create(3);

    // Print node count
    NS_LOG_INFO("Number of nodes created: " << nodes.GetN());

    return 0;
}

