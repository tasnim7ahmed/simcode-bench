#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // Enable logging (optional - comment out if not needed)
    // LogComponentEnable("PointToPointNetDevice", LOG_LEVEL_INFO);

    // Create two nodes
    NodeContainer nodes;
    nodes.Create(2);

    // Configure the point-to-point channel with given attributes
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    // Install net devices on the nodes and connect them with the channel
    NetDeviceContainer devices;
    devices = pointToPoint.Install(nodes);

    // Verification: Print number of nodes and devices after setup
    NS_LOG_UNCOND("Number of nodes: " << NodeList::GetNNodes());
    NS_LOG_UNCOND("Number of net devices: " << devices.GetN());

    // Verify that we have the correct number of devices (should be 2)
    if (devices.GetN() == 2) {
        NS_LOG_UNCOND("Point-to-point link established successfully.");
    } else {
        NS_LOG_UNCOND("Point-to-point link setup failed.");
    }

    // Run the simulation (there is no actual network traffic or events)
    Simulator::Stop(Seconds(1.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}