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

    // Create nodes
    NodeContainer nodes;
    nodes.Create(4); // Create 4 nodes

    // Create a point-to-point link helper
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("1ms"));

    // Install devices and set up point-to-point links in a ring topology
    NetDeviceContainer devices;

    for (uint32_t i = 0; i < nodes.GetN(); ++i)
    {
        // Connect each node to the next, and wrap around for the last node
        NetDeviceContainer temp = pointToPoint.Install(nodes.Get(i), nodes.Get((i + 1) % nodes.GetN()));
        devices.Add(temp);
    }

    // Install the Internet stack on all nodes
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses in the 10.1.x.0/24 subnet
    Ipv4AddressHelper address;
    Ipv4InterfaceContainer interfaces;

    for (uint32_t i = 0; i < nodes.GetN(); ++i)
    {
        address.SetBase("10.1." + std::to_string(i + 1) + ".0", "255.255.255.0");
        interfaces.Add(address.Assign(devices.Get(i)));
    }

    // Application: Sending packets from each node to its clockwise neighbor
    uint16_t port = 9; // Port for communication
    for (uint32_t i = 0; i < nodes.GetN(); ++i)
    {
        // Each node sends packets to its clockwise neighbor
        OnOffHelper onoff("ns3::UdpSocketFactory", InetSocketAddress(interfaces.GetAddress((i + 1) % nodes.GetN()), port));
        onoff.SetConstantRate(DataRate("1Mbps"), 1024); // 1Mbps data rate, 1024-byte packets
        ApplicationContainer app = onoff.Install(nodes.Get(i)); // Install on each node
        app.Start(Seconds(1.0 + i)); // Stagger start times
        app.Stop(Seconds(10.0)); // Stop at 10 seconds
    }

    // Set up a PacketSink on each node to receive packets
    for (uint32_t i = 0; i < nodes.GetN(); ++i)
    {
        PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
        ApplicationContainer sinkApp = sink.Install(nodes.Get(i)); // Install sink on each node
        sinkApp.Start(Seconds(1.0));
        sinkApp.Stop(Seconds(10.0));
    }

    // Enable routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Run the simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}

