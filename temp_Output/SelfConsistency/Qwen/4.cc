#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("PointToPointNetwork");

int
main(int argc, char* argv[])
{
    // Disable logging unless needed
    LogComponentEnable("PointToPointNetwork", LOG_LEVEL_INFO);

    // Create two nodes
    NodeContainer nodes;
    nodes.Create(2);

    // Setup point-to-point link with 5 Mbps data rate and 2 ms delay
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    // Install devices on the nodes
    NetDeviceContainer devices = pointToPoint.Install(nodes);

    // Install Internet stack on all nodes
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses from the network 10.1.1.0/24
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Output assigned IP addresses
    for (uint32_t i = 0; i < interfaces.GetN(); ++i)
    {
        Ptr<Ipv4> ipv4 = nodes.Get(i)->GetObject<Ipv4>();
        Ipv4Address addr = ipv4->GetAddress(1, 0).GetLocal(); // Interface 1 (point-to-point)
        NS_LOG_UNCOND("Node " << i << " IP Address: " << addr);
    }

    // Run the simulation
    Simulator::Run();

    // Destroy the simulation
    Simulator::Destroy();

    return 0;
}