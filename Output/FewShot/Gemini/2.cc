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

    // Install the point-to-point link between the nodes
    NetDeviceContainer devices = pointToPoint.Install(nodes);

    // Run the simulation for a short time to allow the link to initialize
    Simulator::Run(Seconds(0.1));

    // Destroy the simulator
    Simulator::Destroy();

    return 0;
}