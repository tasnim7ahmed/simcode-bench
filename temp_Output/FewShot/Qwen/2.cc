#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Create two nodes
    NodeContainer nodes;
    nodes.Create(2);

    // Configure point-to-point link
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    // Install the link between the nodes
    NetDeviceContainer devices = pointToPoint.Install(nodes);

    // No IP stack, no applications - just verifying link setup

    // Run the simulation for a short duration to allow link setup
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}