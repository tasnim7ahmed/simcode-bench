#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    uint32_t numPeripherals = 4;
    CommandLine cmd;
    cmd.AddValue("n", "Number of peripheral nodes", numPeripherals);
    cmd.Parse(argc, argv);

    uint32_t totalNodes = numPeripherals + 1;

    NodeContainer nodes;
    nodes.Create(totalNodes);

    uint32_t hubIndex = 0;

    std::vector<NetDeviceContainer> deviceContainers;
    std::vector<NodeContainer> linkContainers;

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    for (uint32_t i = 1; i < totalNodes; ++i)
    {
        NodeContainer nc;
        nc.Add(nodes.Get(hubIndex));
        nc.Add(nodes.Get(i));
        linkContainers.push_back(nc);

        NetDeviceContainer ndc = p2p.Install(nc);
        deviceContainers.push_back(ndc);
    }

    for (uint32_t i = 1; i < totalNodes; ++i)
    {
        std::cout << "Peripheral node " << i
                  << " connected to hub node " << hubIndex << " (";
        std::cout << "Link established between Node[" << hubIndex << "] and Node[" << i << "])"
                  << std::endl;
    }

    Simulator::Run();
    Simulator::Destroy();
    return 0;
}