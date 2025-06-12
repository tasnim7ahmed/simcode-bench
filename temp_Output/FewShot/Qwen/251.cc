#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Define the number of nodes in the ring
    uint32_t numNodes = 5;

    // Create nodes
    NodeContainer nodes;
    nodes.Create(numNodes);

    // Create point-to-point helper
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // Install links to form a ring
    for (uint32_t i = 0; i < numNodes; ++i) {
        NodeContainer pair = NodeContainer(nodes.Get(i), nodes.Get((i + 1) % numNodes));
        p2p.Install(pair);
    }

    // Print confirmation of connections
    std::cout << "Ring topology established with " << numNodes << " nodes." << std::endl;

    return 0;
}