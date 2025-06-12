#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/mobility-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Set up logging
    LogComponentEnable("PointToPointNetDevice", LOG_LEVEL_INFO);

    // Create nodes
    NodeContainer nodes;
    uint32_t numNodes = 5;
    nodes.Create(numNodes);

    // Mesh topology setup
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("1ms"));

    for (uint32_t i = 0; i < numNodes; ++i) {
        for (uint32_t j = i + 1; j < numNodes; ++j) {
            NetDeviceContainer devices;
            devices = pointToPoint.Install(nodes.Get(i), nodes.Get(j));

            NS_LOG_INFO("Created direct link between Node " << i << " and Node " << j);
            std::cout << "Link established: Node " << i << " <-> Node " << j << std::endl;
        }
    }

    // Run the simulation (no applications, just topology)
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}