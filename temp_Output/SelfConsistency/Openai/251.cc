#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"

using namespace ns3;

int
main(int argc, char *argv[])
{
    uint32_t numNodes = 6; // Number of nodes in the ring

    CommandLine cmd;
    cmd.AddValue("numNodes", "Number of nodes in the ring topology", numNodes);
    cmd.Parse(argc, argv);

    if (numNodes < 3) {
        std::cerr << "Ring topology needs at least 3 nodes." << std::endl;
        return 1;
    }

    // Create nodes
    NodeContainer nodes;
    nodes.Create(numNodes);

    // Store NetDeviceContainers for reference (not strictly needed here)
    std::vector<NetDeviceContainer> netDevices;

    // Configure PointToPoint link attributes
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // Connect nodes in a ring
    for (uint32_t i = 0; i < numNodes; ++i)
    {
        uint32_t next = (i + 1) % numNodes;

        NodeContainer pair;
        pair.Add(nodes.Get(i));
        pair.Add(nodes.Get(next));

        NetDeviceContainer ndc = p2p.Install(pair);
        netDevices.push_back(ndc);

        std::cout << "Connected node " << i << " <--> node " << next << std::endl;
    }

    Simulator::Stop(Seconds(0.1));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}