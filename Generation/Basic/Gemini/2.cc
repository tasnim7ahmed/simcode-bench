#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"

int
main(int argc, char* argv[])
{
    // Create two nodes
    ns3::NodeContainer nodes;
    nodes.Create(2);

    // Create a PointToPointHelper
    ns3::PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", ns3::StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", ns3::StringValue("2ms"));

    // Install the point-to-point link devices on the nodes
    ns3::NetDeviceContainer devices;
    devices = pointToPoint.Install(nodes);

    // No IP addresses or applications are configured as per the description.
    // The focus is solely on establishing the link.

    // Run the simulation
    ns3::Simulator::Run();

    // Clean up
    ns3::Simulator::Destroy();

    return 0;
}