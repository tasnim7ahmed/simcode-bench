#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TreeTopologyExample");

int main(int argc, char *argv[]) {
    uint32_t numIntermediateNodes = 2;
    uint32_t numLeafNodesPerIntermediate = 2;

    NodeContainer root;
    root.Create(1);

    NodeContainer intermediateNodes;
    intermediateNodes.Create(numIntermediateNodes);

    std::vector<NodeContainer> leafNodes(numIntermediateNodes);

    for (uint32_t i = 0; i < numIntermediateNodes; ++i) {
        leafNodes[i].Create(numLeafNodesPerIntermediate);
    }

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer rootDevices;
    std::vector<NetDeviceContainer> intermediateDevices(numIntermediateNodes);
    std::vector<std::vector<NetDeviceContainer>> leafDevices(numIntermediateNodes);

    for (uint32_t i = 0; i < numIntermediateNodes; ++i) {
        rootDevices = p2p.Install(root.Get(0), intermediateNodes.Get(i));
        intermediateDevices[i] = rootDevices;

        leafDevices[i].resize(numLeafNodesPerIntermediate);
        for (uint32_t j = 0; j < numLeafNodesPerIntermediate; ++j) {
            leafDevices[i][j] = p2p.Install(intermediateNodes.Get(i), leafNodes[i].Get(j));
        }
    }

    InternetStackHelper stack;
    stack.InstallAll();

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");

    for (uint32_t i = 0; i < numIntermediateNodes; ++i) {
        address.Assign(intermediateDevices[i]);
        address.NewNetwork();
        for (uint32_t j = 0; j < numLeafNodesPerIntermediate; ++j) {
            address.Assign(leafDevices[i][j]);
            address.NewNetwork();
        }
    }

    NS_LOG_INFO("Tree topology built.");
    for (uint32_t i = 0; i < numIntermediateNodes; ++i) {
        std::cout << "Root connected to Intermediate node " << i + 1 << std::endl;
        for (uint32_t j = 0; j < numLeafNodesPerIntermediate; ++j) {
            std::cout << "Intermediate node " << i + 1 << " connected to Leaf node " << j + 1 << " under it." << std::endl;
        }
    }

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}