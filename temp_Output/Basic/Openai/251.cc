#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    uint32_t numNodes = 6;

    CommandLine cmd;
    cmd.AddValue("numNodes", "Number of nodes in the ring", numNodes);
    cmd.Parse(argc, argv);

    NodeContainer nodes;
    nodes.Create(numNodes);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("1Gbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    std::vector<NetDeviceContainer> devices(numNodes);

    for (uint32_t i = 0; i < numNodes; ++i)
    {
        uint32_t next = (i + 1) % numNodes;
        NodeContainer pair;
        pair.Add(nodes.Get(i));
        pair.Add(nodes.Get(next));
        devices[i] = p2p.Install(pair);
        std::cout << "Link established between Node " << i << " and Node " << next << std::endl;
    }

    Simulator::Stop(Seconds(0.1));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}