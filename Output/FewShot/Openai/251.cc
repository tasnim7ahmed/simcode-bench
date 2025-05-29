#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    uint32_t numNodes = 5; // Number of nodes in the ring

    CommandLine cmd;
    cmd.AddValue("numNodes", "Number of nodes in the ring topology", numNodes);
    cmd.Parse(argc, argv);

    if (numNodes < 3) {
        NS_ABORT_MSG("Ring requires at least 3 nodes.");
    }

    // Create nodes
    NodeContainer nodes;
    nodes.Create(numNodes);

    // Create point-to-point links for the ring
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    std::vector<NetDeviceContainer> deviceContainers;

    for (uint32_t i = 0; i < numNodes; ++i) {
        uint32_t next = (i + 1) % numNodes;
        NodeContainer pair(nodes.Get(i), nodes.Get(next));
        NetDeviceContainer devices = p2p.Install(pair);
        deviceContainers.push_back(devices);
        std::cout << "Established p2p link between Node " << i
                  << " and Node " << next << std::endl;
    }

    return 0;
}