#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/internet-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    uint32_t numNodes = 6;
    CommandLine cmd;
    cmd.AddValue("numNodes", "Number of nodes in ring topology", numNodes);
    cmd.Parse(argc, argv);

    NodeContainer nodes;
    nodes.Create(numNodes);

    std::vector<NetDeviceContainer> deviceContainers;
    std::vector<PointToPointHelper> p2pLinks;

    for (uint32_t i = 0; i < numNodes; ++i)
    {
        PointToPointHelper p2p;
        p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
        p2p.SetChannelAttribute("Delay", StringValue("2ms"));

        uint32_t next = (i + 1) % numNodes;
        NetDeviceContainer devices = p2p.Install(nodes.Get(i), nodes.Get(next));
        deviceContainers.push_back(devices);
        p2pLinks.push_back(p2p);
    }

    // Print confirmation of the established connections
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