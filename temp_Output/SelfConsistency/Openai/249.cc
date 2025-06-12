#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"

using namespace ns3;

int
main(int argc, char *argv[])
{
    uint32_t numNodes = 5; // Number of nodes in the linear topology

    CommandLine cmd;
    cmd.AddValue("numNodes", "Number of nodes in the linear topology (>=2)", numNodes);
    cmd.Parse(argc, argv);

    if (numNodes < 2)
    {
        NS_LOG_UNCOND("You must have at least two nodes to form a linear topology.");
        return 1;
    }

    // Create nodes
    NodeContainer nodes;
    nodes.Create(numNodes);

    // Create and configure PointToPointHelper
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // Create links between adjacent nodes
    std::vector<NetDeviceContainer> deviceContainers;

    for (uint32_t i = 0; i < numNodes - 1; ++i)
    {
        NodeContainer pair(nodes.Get(i), nodes.Get(i + 1));
        NetDeviceContainer devices = p2p.Install(pair);
        deviceContainers.push_back(devices);

        // Print confirmation of the connection
        NS_LOG_UNCOND("Established p2p link between Node " << i << " and Node " << i + 1);
    }

    Simulator::Stop(Seconds(0.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}