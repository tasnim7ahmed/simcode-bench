#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/node-container.h"
#include "ns3/net-device-container.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("IpAddressAssignmentExample");

int main(int argc, char *argv[]) {
    // Enable logging for this simulation
    LogComponentEnable("IpAddressAssignmentExample", LOG_LEVEL_INFO);

    // Create two nodes
    NodeContainer nodes;
    nodes.Create(2);

    // Setup point-to-point link between the nodes
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices = pointToPoint.Install(nodes);

    // Install Internet stack on the nodes
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses from the 10.1.1.0/24 subnet
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Output assigned IP addresses
    for (uint32_t i = 0; i < interfaces.GetN(); ++i) {
        Ptr<Ipv4> ipv4 = nodes.Get(i)->GetObject<Ipv4>();
        Ipv4Address addr = ipv4->GetAddress(1, 0).GetLocal(); // Interface 1 (only one used here)
        NS_LOG_INFO("Node " << i << " IP Address: " << addr);
    }

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}