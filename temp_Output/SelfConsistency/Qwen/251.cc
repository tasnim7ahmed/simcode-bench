#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/csma-module.h"
#include "ns3/wifi-module.h"
#include "ns3/bridge-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("RingTopologyExample");

int main(int argc, char *argv[]) {
    uint32_t numNodes = 5;

    // Enable logging
    LogComponentEnable("RingTopologyExample", LOG_LEVEL_INFO);

    // Create nodes
    NodeContainer nodes;
    nodes.Create(numNodes);

    // Create point-to-point helper
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // Install internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Connect nodes in a ring
    NetDeviceContainer devices[numNodes];

    for (uint32_t i = 0; i < numNodes; ++i) {
        uint32_t j = (i + 1) % numNodes;
        NodeContainer pair = NodeContainer(nodes.Get(i), nodes.Get(j));
        devices[i] = p2p.Install(pair);
        NS_LOG_INFO("Connected node " << i << " to node " << j);
    }

    // Assign IP addresses
    Ipv4AddressHelper address;
    char ipBase[20];
    for (uint32_t i = 0; i < numNodes; ++i) {
        sprintf(ipBase, "10.1.0.%d", i);
        address.SetBase(ipBase, "255.255.255.0");
        Ipv4InterfaceContainer interfaces = address.Assign(devices[i]);
        NS_LOG_INFO("Assigned IP addresses for link between node " << i << " and node " << (i + 1) % numNodes);
    }

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}