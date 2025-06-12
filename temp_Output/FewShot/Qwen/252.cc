#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Disable logging for brevity; enable if needed for debugging
    LogComponentEnable("PacketSink", LOG_LEVEL_INFO);

    // Create nodes: 1 root, 2 intermediates, 4 leaves (total 7 nodes)
    NodeContainer root;
    root.Create(1);

    NodeContainer intermediates;
    intermediates.Create(2);

    NodeContainer leaves;
    leaves.Create(4);

    // Point-to-point helper to connect nodes
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // Connect root to intermediate nodes
    NetDeviceContainer devicesRootToIntermediate1;
    devicesRootToIntermediate1 = p2p.Install(root.Get(0), intermediates.Get(0));

    NetDeviceContainer devicesRootToIntermediate2;
    devicesRootToIntermediate2 = p2p.Install(root.Get(0), intermediates.Get(1));

    // Connect each intermediate node to two leaf nodes
    NetDeviceContainer devicesIntermediate1ToLeaf0;
    devicesIntermediate1ToLeaf0 = p2p.Install(intermediates.Get(0), leaves.Get(0));

    NetDeviceContainer devicesIntermediate1ToLeaf1;
    devicesIntermediate1ToLeaf1 = p2p.Install(intermediates.Get(0), leaves.Get(1));

    NetDeviceContainer devicesIntermediate2ToLeaf2;
    devicesIntermediate2ToLeaf2 = p2p.Install(intermediates.Get(1), leaves.Get(2));

    NetDeviceContainer devicesIntermediate2ToLeaf3;
    devicesIntermediate2ToLeaf3 = p2p.Install(intermediates.Get(1), leaves.Get(3));

    // Install Internet stack on all nodes
    InternetStackHelper stack;
    stack.InstallAll();

    // Assign IP addresses to all devices
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");

    Ipv4InterfaceContainer interfacesRootIntermediate1;
    interfacesRootIntermediate1 = address.Assign(devicesRootToIntermediate1);

    address.NewNetwork();
    Ipv4InterfaceContainer interfacesRootIntermediate2;
    interfacesRootIntermediate2 = address.Assign(devicesRootToIntermediate2);

    address.NewNetwork();
    Ipv4InterfaceContainer interfacesIntermediate1Leaf0;
    interfacesIntermediate1Leaf0 = address.Assign(devicesIntermediate1ToLeaf0);

    address.NewNetwork();
    Ipv4InterfaceContainer interfacesIntermediate1Leaf1;
    interfacesIntermediate1Leaf1 = address.Assign(devicesIntermediate1ToLeaf1);

    address.NewNetwork();
    Ipv4InterfaceContainer interfacesIntermediate2Leaf2;
    interfacesIntermediate2Leaf2 = address.Assign(devicesIntermediate2ToLeaf2);

    address.NewNetwork();
    Ipv4InterfaceContainer interfacesIntermediate2Leaf3;
    interfacesIntermediate2Leaf3 = address.Assign(devicesIntermediate2ToLeaf3);

    // Print confirmation of connections
    std::cout << "Tree topology established:" << std::endl;
    std::cout << "Root connected to two intermediate nodes." << std::endl;
    std::cout << "Each intermediate node connected to two leaf nodes." << std::endl;

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}