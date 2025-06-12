#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Create two nodes
    NodeContainer nodes;
    nodes.Create(2);

    // Setup point-to-point link attributes
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    // Install the point-to-point devices and channel between the nodes
    NetDeviceContainer devices = pointToPoint.Install(nodes);

    // Ensure there are exactly two devices created
    NS_ASSERT_MSG(devices.GetN() == 2, "Point-to-point link should have exactly two devices.");

    // Log successful setup
    std::cout << "Point-to-point link successfully established between two nodes." << std::endl;

    return 0;
}