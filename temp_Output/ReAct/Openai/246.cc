#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"

using namespace ns3;

int
main(int argc, char *argv[])
{
    // LogComponentEnable ("PointToPointNetDevice", LOG_LEVEL_INFO);

    // Set up simulation parameters
    uint32_t numNodes = 4; // simple example with 4 nodes
    CommandLine cmd;
    cmd.AddValue ("numNodes", "Number of nodes in the topology", numNodes);
    cmd.Parse (argc, argv);

    // Create nodes
    NodeContainer nodes;
    nodes.Create(numNodes);

    // Create point-to-point links between consecutive nodes
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
    p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

    std::vector<NetDeviceContainer> devices;
    for (uint32_t i = 0; i < numNodes - 1; ++i)
    {
        NodeContainer pair = NodeContainer(nodes.Get(i), nodes.Get(i + 1));
        NetDeviceContainer dev = p2p.Install(pair);
        devices.push_back(dev);
        std::cout << "Established P2P link between Node " << i
                  << " (ID: " << nodes.Get(i)->GetId() << ") and Node " << (i+1)
                  << " (ID: " << nodes.Get(i+1)->GetId() << ")" << std::endl;
    }

    Simulator::Stop(Seconds(0.1));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}