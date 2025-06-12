#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/internet-module.h"

using namespace ns3;

int
main(int argc, char *argv[])
{
    uint32_t numNodes = 5;
    CommandLine cmd;
    cmd.AddValue("numNodes", "Number of nodes in the linear topology", numNodes);
    cmd.Parse(argc, argv);

    NodeContainer nodes;
    nodes.Create(numNodes);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    std::vector<NetDeviceContainer> deviceContainers;

    for (uint32_t i = 0; i < numNodes - 1; ++i)
    {
        NodeContainer pair(nodes.Get(i), nodes.Get(i + 1));
        NetDeviceContainer devices = p2p.Install(pair);
        deviceContainers.push_back(devices);

        std::cout << "Established Point-to-Point link between Node " << i
                  << " and Node " << (i + 1) << std::endl;
    }

    Simulator::Stop(Seconds(1.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}