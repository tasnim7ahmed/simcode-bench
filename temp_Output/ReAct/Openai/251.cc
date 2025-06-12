#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    uint32_t numNodes = 5; // Example: create a 5-node ring
    CommandLine cmd;
    cmd.AddValue("numNodes", "Number of nodes in the ring", numNodes);
    cmd.Parse(argc, argv);

    NodeContainer nodes;
    nodes.Create(numNodes);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    std::vector<NetDeviceContainer> devicePairs;

    for (uint32_t i = 0; i < numNodes; ++i)
    {
        uint32_t next = (i + 1) % numNodes;
        NodeContainer pair(nodes.Get(i), nodes.Get(next));
        NetDeviceContainer devs = p2p.Install(pair);
        devicePairs.push_back(devs);
    }

    // Print confirmation
    for (uint32_t i = 0; i < numNodes; ++i)
    {
        uint32_t next = (i + 1) % numNodes;
        std::cout << "Link established between Node " << i << " and Node " << next << std::endl;
    }

    Simulator::Stop(Seconds(1.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}