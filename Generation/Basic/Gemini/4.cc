#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ipv4-address-helper.h"

int
main(int argc, char* argv[])
{
    // Set up default values for point-to-point
    ns3::PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", ns3::StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", ns3::StringValue("2ms"));

    // Create two nodes
    ns3::NodeContainer nodes;
    nodes.Create(2);

    // Install the Internet stack on both nodes
    ns3::InternetStackHelper stack;
    stack.Install(nodes);

    // Create the point-to-point net devices and link them
    ns3::NetDeviceContainer devices;
    devices = pointToPoint.Install(nodes);

    // Assign IP addresses to the devices
    ns3::Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    address.Assign(devices);

    // Run the simulation
    ns3::Simulator::Run();

    // Destroy the simulation
    ns3::Simulator::Destroy();

    return 0;
}