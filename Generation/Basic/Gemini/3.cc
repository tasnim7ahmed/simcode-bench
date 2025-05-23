#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

int
main(int argc, char *argv[])
{
    // Enable logging for relevant modules to observe the setup
    LogComponentEnable("PointToPointNetDevice", LOG_LEVEL_INFO);
    LogComponentEnable("Ipv4InterfaceContainer", LOG_LEVEL_INFO);

    // Create two nodes
    NodeContainer nodes;
    nodes.Create(2);

    // Create a PointToPointHelper and configure link attributes
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // Install Point-to-Point devices on the nodes and connect them
    NetDeviceContainer devices = p2p.Install(nodes);

    // Install the Internet stack on the nodes
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses from the "10.1.1.0/24" range
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Populate routing tables (important for IP functionality, even without active communication)
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // No communication required, so no applications are set up.

    // Run the simulation
    Simulator::Run();

    // Clean up simulation resources
    Simulator::Destroy();

    return 0;
}