#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/internet-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("SimpleP2PTopology");

int main(int argc, char *argv[]) {
    uint32_t numNodes = 4;

    // Create nodes
    NodeContainer nodes;
    nodes.Create(numNodes);

    // Setup point-to-point links between consecutive nodes
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");

    NS_LOG_INFO("Create point-to-point links.");
    for (uint32_t i = 0; i < numNodes - 1; ++i) {
        NodeContainer pair = NodeContainer(nodes.Get(i), nodes.Get(i + 1));
        NetDeviceContainer devices = p2p.Install(pair);
        address.Assign(devices);
        std::cout << "Link established between Node " << nodes.Get(i)->GetId() << " and Node " << nodes.Get(i + 1)->GetId() << std::endl;
    }

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}