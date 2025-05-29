#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"

using namespace ns3;

int
main(int argc, char *argv[])
{
    // Enable logging if needed
    // LogComponentEnable("PointToPointExample", LOG_LEVEL_INFO);

    // Create two nodes
    NodeContainer nodes;
    nodes.Create(2);

    // Create point-to-point helper and configure link attributes
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    // Install net devices on nodes
    NetDeviceContainer devices;
    devices = pointToPoint.Install(nodes);

    // Install the internet stack on both nodes
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses to devices
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Optionally, print assigned IP addresses
    // std::cout << "Node 0 IP: " << interfaces.GetAddress(0) << std::endl;
    // std::cout << "Node 1 IP: " << interfaces.GetAddress(1) << std::endl;

    Simulator::Stop(Seconds(1.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}