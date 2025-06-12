#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/csma-module.h"
#include "ns3/wifi-module.h"
#include "ns3/ssid.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("SimpleIpAssignmentExample");

int main(int argc, char *argv[]) {
    // Enable logging for this example
    LogComponentEnable("SimpleIpAssignmentExample", LOG_LEVEL_INFO);

    // Create 4 nodes
    uint32_t numNodes = 4;
    NodeContainer nodes;
    nodes.Create(numNodes);

    // Install Internet stack on all nodes
    InternetStackHelper internet;
    internet.Install(nodes);

    // Create a point-to-point channel between the nodes to form a chain
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices;
    for (uint32_t i = 0; i < numNodes - 1; ++i) {
        devices.Add(p2p.Install(nodes.Get(i), nodes.Get(i + 1)));
    }

    // Assign IP addresses
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");

    Ipv4InterfaceContainer interfaces;
    for (uint32_t i = 0; i < numNodes - 1; ++i) {
        NetDeviceContainer pair = NetDeviceContainer(devices.Get(2 * i), devices.Get(2 * i + 1));
        Ipv4InterfaceContainer ifc = ipv4.Assign(pair);
        interfaces.Add(ifc);
        ipv4.NewNetwork();
    }

    // Print assigned IP addresses
    for (uint32_t i = 0; i < numNodes; ++i) {
        Ptr<Node> node = nodes.Get(i);
        Ptr<Ipv4> ipv4Node = node->GetObject<Ipv4>();
        Ipv4Address addr = ipv4Node->GetAddress(1, 0).GetLocal(); // assuming one interface per node
        NS_LOG_INFO("Node " << i << " IP Address: " << addr);
    }

    // Cleanup and run simulation (required even if no applications are used)
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}