#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable logging
    LogComponentEnable("NodeIPExample", LOG_LEVEL_INFO);

    // Create a container for 3 nodes
    NodeContainer nodes;
    nodes.Create(3);

    // Install the Internet stack on all nodes
    InternetStackHelper internet;
    internet.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");

    // Assign interfaces and print assigned IPs
    Ipv4InterfaceContainer interfaces = address.Assign(nodes);
    
    for (uint32_t i = 0; i < nodes.GetN(); ++i) {
        Ptr<Ipv4> ipv4 = nodes.Get(i)->GetObject<Ipv4>();
        Ipv4Address ip = ipv4->GetAddress(1, 0);
        NS_LOG_INFO("Node " << i << " assigned IP: " << ip);
    }

    return 0;
}

