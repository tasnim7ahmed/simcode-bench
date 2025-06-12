#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/log.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable logging for this script
    LogComponentEnable("RingTopology", LOG_LEVEL_INFO);

    // Number of nodes in the ring
    int numNodes = 5;

    // Create nodes
    NodeContainer nodes;
    nodes.Create(numNodes);

    // Configure point-to-point links
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    // Create links between nodes to form a ring
    for (int i = 0; i < numNodes; ++i) {
        int nextNodeIndex = (i + 1) % numNodes;
        NetDeviceContainer devices = pointToPoint.Install(nodes.Get(i), nodes.Get(nextNodeIndex));
        NS_LOG_INFO("Created link between node " << i << " and node " << nextNodeIndex);
    }

    NS_LOG_INFO("Ring topology created successfully.");

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}