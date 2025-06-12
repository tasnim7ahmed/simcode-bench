#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("RingTopologyExample");

int main(int argc, char *argv[])
{
    // Log settings
    LogComponentEnable("RingTopologyExample", LOG_LEVEL_INFO);

    // Create 3 nodes
    NodeContainer nodes;
    nodes.Create(3);

    // Set up point-to-point links between the nodes to form a ring
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    // Install the devices on the nodes (Node 0 <-> Node 1, Node 1 <-> Node 2, Node 2 <-> Node 0)
    NetDeviceContainer devices01, devices12, devices20;
    devices01 = pointToPoint.Install(nodes.Get(0), nodes.Get(1));
    devices12 = pointToPoint.Install(nodes.Get(1), nodes.Get(2));
    devices20 = pointToPoint.Install(nodes.Get(2), nodes.Get(0));

    // Install the Internet stack (TCP/IP) on all nodes
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses to each pair of point-to-point links
    Ipv4AddressHelper address;
    Ipv4InterfaceContainer interfaces01, interfaces12, interfaces20;

    address.SetBase("10.1.1.0", "255.255.255.0");
    interfaces01 = address.Assign(devices01);

    address.SetBase("10.1.2.0", "255.255.255.0");
    interfaces12 = address.Assign(devices12);

    address.SetBase("10.1.3.0", "255.255.255.0");
    interfaces20 = address.Assign(devices20);

    // Create an application (OnOff Application) to send packets from node 0 to node 1
    uint16_t port = 9; // Port for communication
    OnOffHelper onoff("ns3::UdpSocketFactory", InetSocketAddress(interfaces01.GetAddress(1), port));
    onoff.SetConstantRate(DataRate("1Mbps"), 1024); // 1Mbps data rate, 1024-byte packets
    ApplicationContainer app = onoff.Install(nodes.Get(0)); // Install on node 0
    app.Start(Seconds(1.0)); // Start at 1 second
    app.Stop(Seconds(10.0)); // Stop at 10 seconds

    // Set up a PacketSink on node 1 to receive packets from node 0
    PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApp = sink.Install(nodes.Get(1)); // Install sink on node 1
    sinkApp.Start(Seconds(1.0));
    sinkApp.Stop(Seconds(10.0));

    // Enable routing between the nodes in the ring
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Run the simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}

