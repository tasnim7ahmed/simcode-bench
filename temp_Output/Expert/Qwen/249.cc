#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/internet-stack-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("LinearTopologyExample");

int main(int argc, char *argv[]) {
    uint32_t numNodes = 5;

    // Create nodes
    NodeContainer nodes;
    nodes.Create(numNodes);

    // Setup point-to-point links between nodes in a linear chain
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    InternetStackHelper stack;
    stack.Install(nodes);

    for (uint32_t i = 0; i < numNodes - 1; ++i) {
        NodeContainer pair = NodeContainer(nodes.Get(i), nodes.Get(i + 1));
        NetDeviceContainer devices = p2p.Install(pair);
    }

    // Print confirmation of connections
    std::cout << "Linear topology established with " << numNodes << " nodes." << std::endl;
    for (uint32_t i = 0; i < numNodes - 1; ++i) {
        std::cout << "Node " << i << " connected to Node " << i + 1 << std::endl;
    }

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}