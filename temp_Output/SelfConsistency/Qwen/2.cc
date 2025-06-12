#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/node-container.h"
#include "ns3/internet-stack-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("BasicPointToPointLink");

int
main(int argc, char *argv[])
{
    // Enable logging for this simulation
    LogComponentEnable("BasicPointToPointLink", LOG_LEVEL_INFO);

    // Create two nodes
    NodeContainer nodes;
    nodes.Create(2);

    // Configure the point-to-point link attributes
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    // Install the point-to-point devices on the nodes
    NetDeviceContainer devices = pointToPoint.Install(nodes);

    // Verify that devices were successfully installed
    if (devices.GetN() != 2)
    {
        NS_LOG_ERROR("Failed to create Point-to-Point link between nodes.");
        return 1;
    }

    NS_LOG_INFO("Successfully created Point-to-Point link between two nodes.");

    // Simulation setup complete; there is no traffic generation, so the simulation ends immediately
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}