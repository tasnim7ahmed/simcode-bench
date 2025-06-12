#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable logging
    LogComponentEnable("LinearTopologyExample", LOG_LEVEL_INFO);

    // Define the number of nodes in the chain
    uint32_t numNodes = 5;

    // Create nodes
    NodeContainer nodes;
    nodes.Create(numNodes);

    // Point-to-Point Helper
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // Create links between consecutive nodes
    for (uint32_t i = 0; i < numNodes - 1; ++i) {
        NodeContainer link(nodes.Get(i), nodes.Get(i + 1));
        NetDeviceContainer devices = p2p.Install(link);
        NS_LOG_INFO("Connected Node " << i << " to Node " << i + 1);
    }

    return 0;
}

