#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("StarTopologyExample");

int main(int argc, char *argv[])
{
    // Log settings
    LogComponentEnable("StarTopologyExample", LOG_LEVEL_INFO);

    // Create 3 nodes: centralNode and two peripheral nodes
    NodeContainer centralNode;
    centralNode.Create(1);
    
    NodeContainer peripheralNodes;
    peripheralNodes.Create(2);

    // Create a point-to-point link helper
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("1ms"));

    // Install devices and set up point-to-point links
    NetDeviceContainer devicesCentralToNode1, devicesCentralToNode2;
    devicesCentralToNode1 = pointToPoint.Install(centralNode.Get(0), peripheralNodes.Get(0)); // Central to Node 1
    devicesCentralToNode2 = pointToPoint.Install(centralNode.Get(0), peripheralNodes.Get(1)); // Central to Node 2

    // Install the Internet stack on all nodes
    InternetStackHelper stack;
    stack.Install(centralNode);
    stack.Install(peripheralNodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    Ipv4InterfaceContainer interfacesCentralToNode1, interfacesCentralToNode2;

    address.SetBase("10.1.1.0", "255.255.255.0");
    interfacesCentralToNode1 = address.Assign(devicesCentralToNode1);

    address.SetBase("10.1.2.0", "255.255.255.0");
    interfacesCentralToNode2 = address.Assign(devicesCentralToNode2);

    // Application: Sending packets from Node 1 to Node 2 via the central node
    uint16_t port = 9; // Port for communication
    OnOffHelper onoff("ns3::UdpSocketFactory", InetSocketAddress(interfacesCentralToNode2.GetAddress(1), port));
    onoff.SetConstantRate(DataRate("2Mbps"), 1024); // 2Mbps data rate, 1024-byte packets
    ApplicationContainer app = onoff.Install(peripheralNodes.Get(0)); // Install on Node 1
    app.Start(Seconds(1.0)); // Start at 1 second
    app.Stop(Seconds(10.0)); // Stop at 10 seconds

    // Set up a PacketSink on Node 2 to receive packets
    PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApp = sink.Install(peripheralNodes.Get(1)); // Install sink on Node 2
    sinkApp.Start(Seconds(1.0));
    sinkApp.Stop(Seconds(10.0));

    // Enable routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Run the simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}

