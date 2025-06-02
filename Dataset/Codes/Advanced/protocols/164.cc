#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/ipv4-list-routing-helper.h"
#include "ns3/ospf-helper.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("OspfWithNetAnimExample");

int main(int argc, char *argv[])
{
    // Log level
    LogComponentEnable("OspfWithNetAnimExample", LOG_LEVEL_INFO);

    // Create 4 nodes
    NodeContainer nodes;
    nodes.Create(4);

    // Set up point-to-point link properties
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    // Install links between nodes in a square topology
    NetDeviceContainer d1d2, d2d3, d3d4, d4d1;
    d1d2 = pointToPoint.Install(nodes.Get(0), nodes.Get(1));
    d2d3 = pointToPoint.Install(nodes.Get(1), nodes.Get(2));
    d3d4 = pointToPoint.Install(nodes.Get(2), nodes.Get(3));
    d4d1 = pointToPoint.Install(nodes.Get(3), nodes.Get(0));

    // Install internet stack on nodes with OSPF routing
    OspfHelper ospfRouting;
    Ipv4ListRoutingHelper listRouting;
    listRouting.Add(ospfRouting, 10); // OSPF priority 10
    InternetStackHelper stack;
    stack.SetRoutingHelper(listRouting);
    stack.Install(nodes);

    // Assign IP addresses to devices
    Ipv4AddressHelper address;
    Ipv4InterfaceContainer interfaces;
    
    address.SetBase("10.1.1.0", "255.255.255.0");
    interfaces.Add(address.Assign(d1d2));
    
    address.SetBase("10.2.1.0", "255.255.255.0");
    interfaces.Add(address.Assign(d2d3));
    
    address.SetBase("10.3.1.0", "255.255.255.0");
    interfaces.Add(address.Assign(d3d4));
    
    address.SetBase("10.4.1.0", "255.255.255.0");
    interfaces.Add(address.Assign(d4d1));

    // Enable OSPF to populate routing tables
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Create TCP traffic from node 1 to node 3
    uint16_t port = 50000;
    OnOffHelper onOffHelper("ns3::TcpSocketFactory", InetSocketAddress(interfaces.GetAddress(2), port));
    onOffHelper.SetAttribute("DataRate", StringValue("5Mbps"));
    onOffHelper.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp = onOffHelper.Install(nodes.Get(0)); // TCP traffic from node 1 to node 3
    clientApp.Start(Seconds(1.0));
    clientApp.Stop(Seconds(10.0));

    // Install a sink on node 3
    PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApp = sinkHelper.Install(nodes.Get(2));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(10.0));

    // Set up NetAnim
    AnimationInterface anim("ospf_with_netanim.xml");

    // Set node positions for better visualization
    anim.SetConstantPosition(nodes.Get(0), 10, 10);
    anim.SetConstantPosition(nodes.Get(1), 30, 10);
    anim.SetConstantPosition(nodes.Get(2), 30, 30);
    anim.SetConstantPosition(nodes.Get(3), 10, 30);

    // Enable packet metadata for visualization
    anim.EnablePacketMetadata(true);

    // Run the simulation
    Simulator::Stop(Seconds(12.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}

