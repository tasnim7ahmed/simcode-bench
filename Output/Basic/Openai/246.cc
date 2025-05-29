#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    CommandLine cmd;
    cmd.Parse(argc, argv);

    // Number of nodes
    uint32_t numNodes = 4;

    // Create nodes
    NodeContainer nodes;
    nodes.Create(numNodes);

    // Install Internet stack
    InternetStackHelper internet;
    internet.Install(nodes);

    // Point-to-Point helper setup
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("1Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // Connect each consecutive node with a P2P link
    for (uint32_t i = 0; i < numNodes - 1; ++i)
    {
        NodeContainer pair(nodes.Get(i), nodes.Get(i+1));
        NetDeviceContainer devices = p2p.Install(pair);

        std::cout << "Established P2P link between node " 
                  << nodes.Get(i)->GetId() << " and node " 
                  << nodes.Get(i+1)->GetId() << std::endl;
    }

    return 0;
}