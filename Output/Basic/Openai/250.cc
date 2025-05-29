#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"

using namespace ns3;

int
main(int argc, char *argv[])
{
    uint32_t nNodes = 5;

    CommandLine cmd;
    cmd.AddValue("nNodes", "Number of mesh nodes", nNodes);
    cmd.Parse(argc, argv);

    if (nNodes < 2)
    {
        NS_ABORT_MSG("Number of nodes must be at least 2");
    }

    NodeContainer nodes;
    nodes.Create(nNodes);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    std::vector<std::vector<NetDeviceContainer>> meshLinks(nNodes, std::vector<NetDeviceContainer>(nNodes));

    for (uint32_t i = 0; i < nNodes; ++i)
    {
        for (uint32_t j = i + 1; j < nNodes; ++j)
        {
            NodeContainer pair;
            pair.Add(nodes.Get(i));
            pair.Add(nodes.Get(j));
            meshLinks[i][j] = p2p.Install(pair);
            NS_LOG_UNCOND("Established Point-to-Point link between Node " << i << " and Node " << j);
        }
    }

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}