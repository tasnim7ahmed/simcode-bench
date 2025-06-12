#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable logging
    LogComponentEnable("StarTopologyExample", LOG_LEVEL_INFO);

    // Define the number of peripheral nodes
    uint32_t numPeripheralNodes = 4;

    // Create nodes: 1 central node + peripheral nodes
    NodeContainer centralNode;
    centralNode.Create(1);

    NodeContainer peripheralNodes;
    peripheralNodes.Create(numPeripheralNodes);

    // Point-to-Point Helper
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // Create links from each peripheral node to the central node
    for (uint32_t i = 0; i < numPeripheralNodes; ++i) {
        NodeContainer link(centralNode.Get(0), peripheralNodes.Get(i));
        NetDeviceContainer devices = p2p.Install(link);
        NS_LOG_INFO("Connected Peripheral Node " << i << " to Central Node 0");
    }

    return 0;
}

