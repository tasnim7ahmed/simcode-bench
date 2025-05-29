#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/internet-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    uint32_t numNodes = 5;

    NodeContainer nodes;
    nodes.Create(numNodes);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    std::vector<NetDeviceContainer> devicesContainers;

    for (uint32_t i = 0; i < numNodes - 1; ++i) {
        NodeContainer pair;
        pair.Add(nodes.Get(i));
        pair.Add(nodes.Get(i + 1));
        NetDeviceContainer devices = pointToPoint.Install(pair);
        devicesContainers.push_back(devices);
        std::cout << "Established Point-to-Point link between node " << i
                  << " and node " << (i + 1) << std::endl;
    }

    Simulator::Run();
    Simulator::Destroy();
    return 0;
}