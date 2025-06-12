#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/node-container.h"
#include "ns3/net-device-container.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Create two nodes
    NodeContainer nodes;
    nodes.Create(2);

    // Setup point-to-point link with 5 Mbps data rate and 2 ms propagation delay
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // Install devices on the nodes
    NetDeviceContainer devices = p2p.Install(nodes);

    // Install Internet stack on all nodes
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses from the 10.1.1.0/24 subnet
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}