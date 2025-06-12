#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"

using namespace ns3;

int 
main(int argc, char *argv[])
{
    // Topology parameters
    uint32_t nIntermediate = 2;
    uint32_t nLeafPerIntermediate = 2;

    NodeContainer rootNode;
    rootNode.Create(1);

    NodeContainer intermediateNodes;
    intermediateNodes.Create(nIntermediate);

    std::vector<NodeContainer> leafNodes(nIntermediate);
    for (uint32_t i = 0; i < nIntermediate; ++i)
    {
        leafNodes[i].Create(nLeafPerIntermediate);
    }

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    std::vector<NetDeviceContainer> devices;

    // Root to Intermediate connections
    for (uint32_t i = 0; i < nIntermediate; ++i)
    {
        NodeContainer pair(rootNode.Get(0), intermediateNodes.Get(i));
        NetDeviceContainer dev = p2p.Install(pair);
        devices.push_back(dev);
        NS_LOG_UNCOND("Connection: Root node 0 <-> Intermediate node " << (i+1));
    }

    // Intermediate to Leaf connections
    for (uint32_t i = 0; i < nIntermediate; ++i)
    {
        for (uint32_t j = 0; j < nLeafPerIntermediate; ++j)
        {
            NodeContainer pair(intermediateNodes.Get(i), leafNodes[i].Get(j));
            NetDeviceContainer dev = p2p.Install(pair);
            devices.push_back(dev);
            NS_LOG_UNCOND("Connection: Intermediate node " << (i+1) 
                           << " <-> Leaf node " << (nIntermediate + i * nLeafPerIntermediate + j + 1));
        }
    }

    Simulator::Stop(Seconds(0.1));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}