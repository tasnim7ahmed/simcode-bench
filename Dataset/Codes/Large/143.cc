#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TreeTopologyExample");

int main(int argc, char *argv[])
{
    // Log settings
    LogComponentEnable("TreeTopologyExample", LOG_LEVEL_INFO);

    // Create nodes
    NodeContainer rootNode;
    rootNode.Create(1);
    
    NodeContainer childNodes;
    childNodes.Create(2);

    NodeContainer leafNode;
    leafNode.Create(1);

    // Create a point-to-point link helper
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    // Install devices and set up point-to-point links
    NetDeviceContainer devicesRootToChild1, devicesRootToChild2, devicesChildToLeaf;

    // Link from root to child 1
    devicesRootToChild1 = pointToPoint.Install(rootNode.Get(0), childNodes.Get(0));

    // Link from root to child 2
    devicesRootToChild2 = pointToPoint.Install(rootNode.Get(0), childNodes.Get(1));

    // Link from child 1 to leaf node
    devicesChildToLeaf = pointToPoint.Install(childNodes.Get(0), leafNode.Get(0));

    // Install the Internet stack on all nodes
    InternetStackHelper stack;
    stack.Install(rootNode);
    stack.Install(childNodes);
    stack.Install(leafNode);

    // Assign IP addresses
    Ipv4AddressHelper address;
    Ipv4InterfaceContainer interfacesRootToChild1, interfacesRootToChild2, interfacesChildToLeaf;

    // IP addresses for the root to child 1 link
    address.SetBase("10.1.1.0", "255.255.255.0");
    interfacesRootToChild1 = address.Assign(devicesRootToChild1);

    // IP addresses for the root to child 2 link
    address.SetBase("10.1.2.0", "255.255.255.0");
    interfacesRootToChild2 = address.Assign(devicesRootToChild2);

    // IP addresses for the child 1 to leaf link
    address.SetBase("10.1.3.0", "255.255.255.0");
    interfacesChildToLeaf = address.Assign(devicesChildToLeaf);

    // Application: Sending packets from the leaf node to the root node
    uint16_t port = 9; // Port for communication
    OnOffHelper onoff("ns3::UdpSocketFactory", InetSocketAddress(interfacesRootToChild1.GetAddress(0), port));
    onoff.SetConstantRate(DataRate("1Mbps"), 1024); // 1Mbps data rate, 1024-byte packets
    ApplicationContainer app = onoff.Install(leafNode.Get(0)); // Install on the leaf node
    app.Start(Seconds(1.0)); // Start at 1 second
    app.Stop(Seconds(10.0)); // Stop at 10 seconds

    // Set up a PacketSink on the root node to receive packets
    PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApp = sink.Install(rootNode.Get(0)); // Install sink on the root node
    sinkApp.Start(Seconds(1.0));
    sinkApp.Stop(Seconds(10.0));

    // Enable routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Run the simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}

