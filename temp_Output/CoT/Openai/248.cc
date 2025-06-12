#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    uint32_t numPeripherals = 4;

    CommandLine cmd;
    cmd.AddValue("numPeripherals", "Number of peripheral nodes", numPeripherals);
    cmd.Parse(argc, argv);

    NodeContainer hubNode;
    hubNode.Create(1);

    NodeContainer peripheralNodes;
    peripheralNodes.Create(numPeripherals);

    // Vector to store peripheral-hub node pairs
    std::vector<NodeContainer> starLinks;

    // Point-to-Point link attributes
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    std::vector<NetDeviceContainer> deviceContainers;
    for (uint32_t i = 0; i < numPeripherals; ++i)
    {
        NodeContainer link;
        link.Add(hubNode.Get(0));
        link.Add(peripheralNodes.Get(i));
        starLinks.push_back(link);

        NetDeviceContainer devices = p2p.Install(link);
        deviceContainers.push_back(devices);
    }

    InternetStackHelper stack;
    stack.Install(hubNode);
    stack.Install(peripheralNodes);

    // Confirmation output
    std::cout << "Star topology established with " << numPeripherals << " peripheral nodes and 1 hub node." << std::endl;
    for (uint32_t i = 0; i < numPeripherals; ++i)
    {
        std::cout << "Peripheral node " << i << " connected to hub node via Point-to-Point link." << std::endl;
    }

    Simulator::Stop(Seconds(1.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}