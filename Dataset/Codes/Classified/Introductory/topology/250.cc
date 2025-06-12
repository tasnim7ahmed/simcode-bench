#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable logging
    LogComponentEnable("MeshTopologyExample", LOG_LEVEL_INFO);

    // Define the number of nodes in the mesh
    uint32_t numNodes = 4;

    // Create nodes
    NodeContainer nodes;
    nodes.Create(numNodes);

    // Point-to-Point Helper
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // Create full mesh connections
    for (uint32_t i = 0; i < numNodes; ++i) {
        for (uint32_t j = i + 1; j < numNodes; ++j) {
            NodeContainer link(nodes.Get(i), nodes.Get(j));
            NetDeviceContainer devices = p2p.Install(link);
            NS_LOG_INFO("Connected Node " << i << " to Node " << j);
        }
    }

    return 0;
}

