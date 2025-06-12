#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/internet-module.h"

using namespace ns3;

int
main(int argc, char *argv[])
{
    uint32_t numPeripherals = 5;

    CommandLine cmd;
    cmd.AddValue("numPeripherals", "Number of peripheral nodes", numPeripherals);
    cmd.Parse(argc, argv);

    NodeContainer hubNode;
    hubNode.Create(1);

    NodeContainer peripheralNodes;
    peripheralNodes.Create(numPeripherals);

    std::vector<NodeContainer> nodePairs;
    for (uint32_t i = 0; i < numPeripherals; ++i)
    {
        NodeContainer pair;
        pair.Add(hubNode.Get(0));
        pair.Add(peripheralNodes.Get(i));
        nodePairs.push_back(pair);
    }

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    std::vector<NetDeviceContainer> devices;
    for (uint32_t i = 0; i < numPeripherals; ++i)
    {
        devices.push_back(p2p.Install(nodePairs[i]));
    }

    for (uint32_t i = 0; i < numPeripherals; ++i)
    {
        std::cout << "Peripheral node " << peripheralNodes.Get(i)->GetId()
                  << " is connected to hub node " << hubNode.Get(0)->GetId() << std::endl;
    }

    Simulator::Stop(Seconds(0.1));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}