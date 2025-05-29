#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Set up the simulation parameters
    uint32_t nIntermediate = 2;
    uint32_t nLeafPerIntermediate = 2;

    // Node containers
    NodeContainer rootNode;
    rootNode.Create(1);

    NodeContainer intermediateNodes;
    intermediateNodes.Create(nIntermediate);

    std::vector<NodeContainer> leafNodes(nIntermediate);
    for (uint32_t i = 0; i < nIntermediate; ++i) {
        leafNodes[i].Create(nLeafPerIntermediate);
    }

    // Internet stack install for possible later expansion
    InternetStackHelper stack;
    stack.Install(rootNode);
    stack.Install(intermediateNodes);
    for (uint32_t i = 0; i < nIntermediate; ++i) {
        stack.Install(leafNodes[i]);
    }

    // Set up point-to-point helper
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // Root to Intermediate connections
    for (uint32_t i = 0; i < nIntermediate; ++i) {
        NodeContainer link;
        link.Add(rootNode.Get(0));
        link.Add(intermediateNodes.Get(i));
        NetDeviceContainer devices = p2p.Install(link);
        NS_LOG_UNCOND("Established P2P link: Root <-> Intermediate " << i);
    }

    // Intermediate to Leaf connections
    for (uint32_t i = 0; i < nIntermediate; ++i) {
        for (uint32_t j = 0; j < nLeafPerIntermediate; ++j) {
            NodeContainer link;
            link.Add(intermediateNodes.Get(i));
            link.Add(leafNodes[i].Get(j));
            NetDeviceContainer devices = p2p.Install(link);
            NS_LOG_UNCOND("Established P2P link: Intermediate " << i << " <-> Leaf " << i << "-" << j);
        }
    }

    Simulator::Stop(Seconds(1.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}