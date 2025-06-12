#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/internet-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("SimpleP2PTopology");

int main(int argc, char *argv[]) {
    // Enable logging
    LogComponentEnable("SimpleP2PTopology", LOG_LEVEL_INFO);

    // Create two nodes
    NodeContainer nodes;
    nodes.Create(2);

    // Assign unique node IDs
    for (uint32_t i = 0; i < nodes.GetN(); ++i) {
        nodes.Get(i)->SetId(i);
        NS_LOG_INFO("Node created with ID: " << i);
    }

    // Create a point-to-point channel between the nodes
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices = pointToPoint.Install(nodes);

    // Install internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    NS_LOG_INFO("Point-to-Point link established between Node 0 and Node 1.");

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}