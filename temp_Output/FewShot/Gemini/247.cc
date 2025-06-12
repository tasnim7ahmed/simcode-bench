#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Create 5 nodes
    NodeContainer nodes;
    nodes.Create(5);

    // Install Internet Stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Create a PointToPoint link between all nodes for address assignment.
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // Create devices and assign IP addresses
    NetDeviceContainer devices;
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");

    for (uint32_t i = 0; i < nodes.GetN(); ++i) {
        NetDeviceContainer device = p2p.Install(nodes.Get(i), nodes.Get((i + 1) % nodes.GetN()));
        devices.Add(device.Get(0));
        devices.Add(device.Get(1));
    }

    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Print the assigned IP addresses
    for (uint32_t i = 0; i < nodes.GetN(); ++i) {
        Ipv4Address ipAddr = interfaces.GetAddress(i*2);
        std::cout << "Node " << i << " IP Address: " << ipAddr << std::endl;
    }

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}