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

    // Create point-to-point links
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");

    for (uint32_t i = 0; i < numNodes - 1; ++i) {
        NodeContainer pair = NodeContainer(nodes.Get(i), nodes.Get(i + 1));
        NetDeviceContainer devices = p2p.Install(pair);
        address.Assign(devices);
        address.NewNetwork();
    }

    std::cout << "Linear topology with " << numNodes << " nodes created successfully." << std::endl;

    Simulator::Destroy();
    return 0;
}