#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/internet-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    uint32_t numNodes = 4;
    CommandLine cmd;
    cmd.AddValue("numNodes", "Number of mesh nodes", numNodes);
    cmd.Parse(argc, argv);

    NodeContainer nodes;
    nodes.Create(numNodes);

    InternetStackHelper internet;
    internet.Install(nodes);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    uint32_t linkCount = 0;
    for (uint32_t i = 0; i < numNodes; ++i)
    {
        for (uint32_t j = i + 1; j < numNodes; ++j)
        {
            NodeContainer pair(nodes.Get(i), nodes.Get(j));
            NetDeviceContainer devices = p2p.Install(pair);

            std::cout << "Established Point-to-Point link between Node " << i
                      << " and Node " << j << std::endl;
            linkCount++;
        }
    }

    std::cout << "Total links established: " << linkCount << std::endl;

    Simulator::Stop(Seconds(1.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}