#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Set the number of nodes in the linear topology
    uint32_t nNodes = 5;

    // Create a container for all nodes
    NodeContainer nodes;
    nodes.Create(nNodes);

    // Create and configure PointToPointHelper
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    // Install links between consecutive nodes (forming a chain)
    for (uint32_t i = 0; i < nNodes - 1; ++i) {
        NodeContainer pair = NodeContainer(nodes.Get(i), nodes.Get(i + 1));
        pointToPoint.Install(pair);
    }

    // Print confirmation of connections
    std::cout << "Linear topology created with " << nNodes << " nodes connected in a chain." << std::endl;

    return 0;
}