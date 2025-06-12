#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/internet-module.h"
#include "ns3/netanim-module.h"
#include "ns3/mobility-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TreeTopologyExample");

int main(int argc, char *argv[]) {
    uint32_t numIntermediateNodes = 2;
    uint32_t numLeafNodesPerIntermediate = 2;

    CommandLine cmd(__FILE__);
    cmd.AddValue("numIntermediate", "Number of intermediate nodes", numIntermediateNodes);
    cmd.AddValue("numLeavesPerIntermediate", "Number of leaf nodes per intermediate node", numLeafNodesPerIntermediate);
    cmd.Parse(argc, argv);

    NS_LOG_INFO("Creating nodes.");
    NodeContainer root;
    root.Create(1);

    NodeContainer intermediateNodes;
    intermediateNodes.Create(numIntermediateNodes);

    NodeContainer leafNodes;
    leafNodes.Create(numIntermediateNodes * numLeafNodesPerIntermediate);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NS_LOG_INFO("Building the tree topology.");

    NetDeviceContainer rootToIntermediateDevices;
    for (uint32_t i = 0; i < intermediateNodes.GetN(); ++i) {
        NodeContainer pair(root.Get(0), intermediateNodes.Get(i));
        NetDeviceContainer devices = p2p.Install(pair);
        rootToIntermediateDevices.Add(devices);
    }

    NetDeviceContainer intermediateToLeafDevices;
    uint32_t leafIndex = 0;
    for (uint32_t i = 0; i < intermediateNodes.GetN(); ++i) {
        Ptr<Node> intermediate = intermediateNodes.Get(i);
        for (uint32_t j = 0; j < numLeafNodesPerIntermediate; ++j) {
            NodeContainer pair(intermediate, leafNodes.Get(leafIndex++));
            NetDeviceContainer devices = p2p.Install(pair);
            intermediateToLeafDevices.Add(devices);
        }
    }

    NS_LOG_INFO("Assigning IP addresses.");
    InternetStackHelper stack;
    stack.InstallAll();

    Ipv4AddressHelper address;
    address.SetBase("10.1.0.0", "255.255.255.0");

    for (uint32_t i = 0; i < rootToIntermediateDevices.GetN(); i += 2) {
        address.Assign(NetDeviceContainer(rootToIntermediateDevices.Get(i), rootToIntermediateDevices.Get(i + 1)));
        address.NewNetwork();
    }

    for (uint32_t i = 0; i < intermediateToLeafDevices.GetN(); i += 2) {
        address.Assign(NetDeviceContainer(intermediateToLeafDevices.Get(i), intermediateToLeafDevices.Get(i + 1)));
        address.NewNetwork();
    }

    NS_LOG_INFO("Tree topology built and IP addresses assigned.");

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}