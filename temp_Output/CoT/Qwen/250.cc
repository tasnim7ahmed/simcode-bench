#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/internet-stack-helper.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    uint32_t numNodes = 5;

    // Create nodes
    NodeContainer nodes;
    nodes.Create(numNodes);

    // Setup Point-to-Point links between every pair of nodes
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("1ms"));

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.0.0", "255.255.0.0");

    for (uint32_t i = 0; i < numNodes; ++i) {
        for (uint32_t j = i + 1; j < numNodes; ++j) {
            NodeContainer pair = NodeContainer(nodes.Get(i), nodes.Get(j));
            NetDeviceContainer devices = p2p.Install(pair);
            Ipv4InterfaceContainer interfaces = address.Assign(devices);
            address.NewNetwork();
            std::cout << "Connected node " << i << " to node " << j << std::endl;
        }
    }

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}