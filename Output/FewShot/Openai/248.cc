#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    uint32_t numPeripheral = 5; // Number of peripheral nodes

    // Create hub node (central node)
    Ptr<Node> hubNode = CreateObject<Node>();

    // Create peripheral nodes
    NodeContainer peripheralNodes;
    peripheralNodes.Create(numPeripheral);

    // Create a container to hold all nodes (optional for stack installation)
    NodeContainer allNodes;
    allNodes.Add(hubNode);
    allNodes.Add(peripheralNodes);

    // Install Internet stack on all nodes (optional for future use)
    InternetStackHelper internet;
    internet.Install(allNodes);

    // Set Point-to-Point attributes
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // Connect each peripheral node to the hub node
    std::vector<NetDeviceContainer> deviceContainers;
    for (uint32_t i = 0; i < numPeripheral; ++i) {
        NodeContainer link;
        link.Add(hubNode);
        link.Add(peripheralNodes.Get(i));
        NetDeviceContainer devices = p2p.Install(link);
        deviceContainers.push_back(devices);
        std::cout << "Established Point-to-Point connection between Hub (node 0) and Peripheral node " 
                  << (i + 1) << std::endl;
    }

    // Optionally, assign IP addresses here if needed

    Simulator::Stop(Seconds(1.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}