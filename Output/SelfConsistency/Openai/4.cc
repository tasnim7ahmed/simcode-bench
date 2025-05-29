#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"

using namespace ns3;

int
main(int argc, char *argv[])
{
    // Enable logging if necessary (optional)
    // LogComponentEnable("PointToPointExample", LOG_LEVEL_INFO);

    // Create two nodes
    NodeContainer nodes;
    nodes.Create(2);

    // Create point-to-point link attributes
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // Install network devices to the nodes
    NetDeviceContainer devices = p2p.Install(nodes);

    // Install the internet stack (includes IP protocol)
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses to the devices
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // (No communication/application needed)

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}