#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("SimpleIpAssignment");

int main(int argc, char *argv[]) {
    uint32_t numNodes = 4;

    // Create nodes
    NodeContainer nodes;
    nodes.Create(numNodes);

    // Setup point-to-point links between nodes in a chain topology
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices;
    for (uint32_t i = 0; i < numNodes - 1; ++i) {
        devices.Add(p2p.Install(nodes.Get(i), nodes.Get(i + 1)));
    }

    // Install Internet Stack on all nodes
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");

    Ipv4InterfaceContainer interfaces;
    for (uint32_t i = 0; i < numNodes - 1; ++i) {
        NetDeviceContainer pair = devices.Get(i);
        Ipv4InterfaceContainer ic = address.Assign(pair);
        interfaces.Add(ic);
        address.NewNetwork();
    }

    // Print assigned IP addresses
    for (uint32_t i = 0; i < nodes.GetN(); ++i) {
        Ptr<Node> node = nodes.Get(i);
        Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
        for (uint32_t j = 0; j < ipv4->GetNInterfaces(); ++j) {
            Ipv4InterfaceAddress iaddr = ipv4->GetAddress(j, 0);
            Ipv4Address addr = iaddr.GetLocal();
            NS_LOG_UNCOND("Node " << i << " IP: " << addr);
        }
    }

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}