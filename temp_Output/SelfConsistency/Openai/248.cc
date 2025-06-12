#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // Simulation parameters
    uint32_t numPeripheralNodes = 4;

    CommandLine cmd;
    cmd.AddValue("numPeripheralNodes", "Number of peripheral nodes in the star", numPeripheralNodes);
    cmd.Parse(argc, argv);

    // Create central and peripheral nodes
    Ptr<Node> hubNode = CreateObject<Node>();
    NodeContainer peripheralNodes;
    peripheralNodes.Create(numPeripheralNodes);

    // Install Internet Stack (optional, not needed for topology-only setup)
    // InternetStackHelper stack;
    // stack.Install(hubNode);
    // stack.Install(peripheralNodes);

    // Set up Point-to-Point links
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // Store NetDevice containers for possible later use
    std::vector<NetDeviceContainer> deviceContainers;

    // Connect each peripheral node to the hub
    for (uint32_t i = 0; i < numPeripheralNodes; ++i)
    {
        NodeContainer pair(hubNode, peripheralNodes.Get(i));
        NetDeviceContainer devices = p2p.Install(pair);
        deviceContainers.push_back(devices);

        std::cout << "Established Point-to-Point link between Hub (Node " 
                  << hubNode->GetId() << ") and Peripheral Node (Node " 
                  << peripheralNodes.Get(i)->GetId() << ")" << std::endl;
    }

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}