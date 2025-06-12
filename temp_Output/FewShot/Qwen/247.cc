#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Create three nodes
    NodeContainer nodes;
    nodes.Create(3);

    // Install Internet stack on all nodes
    InternetStackHelper stack;
    stack.Install(nodes);

    // Create a simple point-to-point channel (not used for communication, just to assign addresses)
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices;
    for (uint32_t i = 0; i < nodes.GetN() - 1; ++i) {
        NetDeviceContainer pair = p2p.Install(nodes.Get(i), nodes.Get(i + 1));
        devices.Add(pair);
    }

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Print assigned IP addresses
    std::cout << "Assigned IP Addresses:" << std::endl;
    for (uint32_t i = 0; i < interfaces.GetN(); ++i) {
        Ptr<Ipv4> ipv4 = nodes.Get(i)->GetObject<Ipv4>();
        Ipv4Address addr = ipv4->GetAddress(1, 0).GetLocal(); // Get the primary address of interface 1
        std::cout << "Node " << i << ": " << addr << std::endl;
    }

    return 0;
}