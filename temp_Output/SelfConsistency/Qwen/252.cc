#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/node-container.h"
#include "ns3/net-device-container.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TreeTopologyExample");

int main(int argc, char *argv[]) {
    // Log level for debugging output
    LogComponentEnable("TreeTopologyExample", LOG_LEVEL_INFO);

    // Create the root node (level 0)
    NodeContainer root;
    root.Create(1);
    NS_LOG_INFO("Root node created.");

    // Create intermediate nodes (level 1)
    NodeContainer intermediates;
    intermediates.Create(2);
    NS_LOG_INFO("Intermediate nodes created.");

    // Create leaf nodes (level 2), two per intermediate node
    NodeContainer leaves;
    leaves.Create(4);
    NS_LOG_INFO("Leaf nodes created.");

    // Setup Point-to-Point links
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices;

    // Connect root to intermediate nodes
    devices.Add(p2p.Install(root.Get(0), intermediates.Get(0)));
    devices.Add(p2p.Install(root.Get(0), intermediates.Get(1)));
    NS_LOG_INFO("Root connected to intermediate nodes.");

    // Connect each intermediate node to two leaf nodes
    devices.Add(p2p.Install(intermediates.Get(0), leaves.Get(0)));
    devices.Add(p2p.Install(intermediates.Get(0), leaves.Get(1)));
    devices.Add(p2p.Install(intermediates.Get(1), leaves.Get(2)));
    devices.Add(p2p.Install(intermediates.Get(1), leaves.Get(3)));
    NS_LOG_INFO("Intermediate nodes connected to leaf nodes.");

    // Assign IP addresses (not necessary for topology confirmation, but common in setups)
    InternetStackHelper stack;
    stack.InstallAll();

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    for (auto i = devices.Begin(); i != devices.End(); ++i) {
        address.Assign(*i);
        address.NewNetwork();
    }

    NS_LOG_INFO("IP addresses assigned to devices.");

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}