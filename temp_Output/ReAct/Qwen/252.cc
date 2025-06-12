#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/internet-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TreeTopologyExample");

int main(int argc, char *argv[]) {
    uint32_t numIntermediate = 2;
    uint32_t numLeafPerIntermediate = 3;

    NodeContainer root;
    root.Create(1);

    NodeContainer intermediates;
    intermediates.Create(numIntermediate);

    std::vector<NodeContainer> leafs(numIntermediate);
    for (uint32_t i = 0; i < numIntermediate; ++i) {
        leafs[i].Create(numLeafPerIntermediate);
    }

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer rootDevices;
    std::vector<NetDeviceContainer> intermediateDevices(numIntermediate);
    std::vector<std::vector<NetDeviceContainer>> leafDevices(numIntermediate);

    for (uint32_t i = 0; i < numIntermediate; ++i) {
        rootDevices.Add(p2p.Install(root.Get(0), intermediates.Get(i)));
        intermediateDevices[i].Add(rootDevices.Get(rootDevices.GetN() - 1));

        leafDevices[i].resize(numLeafPerIntermediate);
        for (uint32_t j = 0; j < numLeafPerIntermediate; ++j) {
            leafDevices[i][j] = p2p.Install(intermediates.Get(i), leafs[i].Get(j));
            intermediateDevices[i].Add(leafDevices[i][j].Get(0));
        }
    }

    InternetStackHelper stack;
    stack.InstallAll();

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");

    for (uint32_t i = 0; i < rootDevices.GetN(); i += 2) {
        address.Assign(NetDeviceContainer(rootDevices.Get(i), rootDevices.Get(i + 1)));
        address.NewNetwork();
    }

    for (uint32_t i = 0; i < numIntermediate; ++i) {
        for (uint32_t j = 0; j < leafDevices[i].size(); ++j) {
            address.Assign(leafDevices[i][j]);
            address.NewNetwork();
        }
    }

    NS_LOG_INFO("Tree topology created.");
    std::cout << "Tree Topology Established:" << std::endl;
    std::cout << "Root node connected to " << numIntermediate << " intermediate nodes." << std::endl;
    for (uint32_t i = 0; i < numIntermediate; ++i) {
        std::cout << "Intermediate node " << i << " connected to " << numLeafPerIntermediate << " leaf nodes." << std::endl;
    }

    Simulator::Run();
    Simulator::Destroy();
    return 0;
}