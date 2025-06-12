#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/internet-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("P2PNetworkExample");

int main(int argc, char *argv[]) {
    // Enable logging
    LogComponentEnable("P2PNetworkExample", LOG_LEVEL_INFO);

    // Create nodes
    uint32_t numNodes = 4;
    NodeContainer nodes;
    nodes.Create(numNodes);

    // Assign unique node IDs
    for (uint32_t i = 0; i < numNodes; ++i) {
        nodes.Get(i)->SetId(i);
        NS_LOG_INFO("Node ID " << nodes.Get(i)->GetId() << " created.");
    }

    // Setup Point-to-Point links between adjacent nodes
    PointToPointHelper p2pHelper;
    p2pHelper.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2pHelper.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices[numNodes - 1];
    for (uint32_t i = 0; i < numNodes - 1; ++i) {
        devices[i] = p2pHelper.Install(nodes.Get(i), nodes.Get(i + 1));
        NS_LOG_INFO("Link established between Node " << i << " and Node " << i + 1);
    }

    // Assign IP addresses (not used for communication in this example)
    InternetStackHelper internet;
    internet.Install(nodes);

    Ipv4AddressHelper ipv4;
    for (uint32_t i = 0; i < numNodes - 1; ++i) {
        std::ostringstream subnet;
        subnet << "10.1." << i << ".0";
        ipv4.SetBase(subnet.str().c_str(), "255.255.255.0");
        ipv4.Assign(devices[i]);
    }

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}