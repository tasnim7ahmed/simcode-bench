#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/internet-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    uint32_t numNodes = 4;

    CommandLine cmd;
    cmd.AddValue("numNodes", "Number of nodes in the mesh topology", numNodes);
    cmd.Parse(argc, argv);

    NodeContainer nodes;
    nodes.Create(numNodes);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    std::vector<NetDeviceContainer> deviceContainers;

    for (uint32_t i = 0; i < numNodes; ++i)
    {
        for (uint32_t j = i + 1; j < numNodes; ++j)
        {
            NodeContainer pair(nodes.Get(i), nodes.Get(j));
            NetDeviceContainer devices = p2p.Install(pair);
            deviceContainers.push_back(devices);
            std::cout << "Established Point-to-Point connection between Node " << i << " and Node " << j << std::endl;
        }
    }

    Simulator::Stop(Seconds(1.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}