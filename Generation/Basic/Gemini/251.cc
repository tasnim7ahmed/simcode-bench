#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include <iostream>

using namespace ns3;

int main()
{
    uint32_t numNodes = 5;

    NodeContainer nodes;
    nodes.Create(numNodes);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    std::cout << "Creating a ring topology with " << numNodes << " nodes." << std::endl;

    for (uint32_t i = 0; i < numNodes; ++i)
    {
        Ptr<Node> node1 = nodes.Get(i);
        Ptr<Node> node2 = nodes.Get((i + 1) % numNodes);

        NetDeviceContainer devices = p2p.Install(node1, node2);
        
        std::cout << "  Connected Node " << node1->GetId() << " to Node " << node2->GetId() << std::endl;
    }

    std::cout << "Ring topology setup complete." << std::endl;

    return 0;
}