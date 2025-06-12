#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/internet-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    uint32_t numNodes = 5;

    NodeContainer nodes;
    nodes.Create(numNodes);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices[numNodes];

    for (uint32_t i = 0; i < numNodes; ++i) {
        NodeContainer pair = NodeContainer(nodes.Get(i), nodes.Get((i + 1) % numNodes));
        devices[i] = p2p.Install(pair);
    }

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    char ipBase[16];
    for (uint32_t i = 0; i < numNodes; ++i) {
        sprintf(ipBase, "10.1.%d.0", i);
        address.SetBase(ipBase, "255.255.255.0");
        address.Assign(devices[i]);
    }

    Simulator::Run();
    Simulator::Destroy();

    std::cout << "Ring topology with " << numNodes << " nodes created successfully." << std::endl;
    return 0;
}