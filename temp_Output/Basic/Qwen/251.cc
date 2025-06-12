#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/internet-stack-helper.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    uint32_t numNodes = 5;

    NodeContainer nodes;
    nodes.Create(numNodes);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    InternetStackHelper stack;
    stack.Install(nodes);

    NetDeviceContainer devices;
    for (uint32_t i = 0; i < numNodes - 1; ++i) {
        NetDeviceContainer link = p2p.Install(NodeContainer(nodes.Get(i), nodes.Get(i + 1)));
        devices.Add(link);
    }

    NetDeviceContainer loopLink = p2p.Install(NodeContainer(nodes.Get(numNodes - 1), nodes.Get(0)));
    devices.Add(loopLink);

    std::cout << "Ring topology established with " << numNodes << " nodes." << std::endl;
    std::cout << "Total connections (Point-to-Point Links): " << devices.GetN() << std::endl;

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}