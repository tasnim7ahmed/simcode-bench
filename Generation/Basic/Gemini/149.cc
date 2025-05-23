#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"

int main(int argc, char *argv[])
{
    // Create 3 nodes
    ns3::NodeContainer nodes;
    nodes.Create(3); // Node 0, Node 1, Node 2

    // Configure Point-to-Point Helper
    ns3::PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", ns3::StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", ns3::StringValue("2ms"));

    // Install net devices for Node0-Node1 link
    ns3::NetDeviceContainer devices01 = pointToPoint.Install(nodes.Get(0), nodes.Get(1));
    // Install net devices for Node1-Node2 link
    ns3::NetDeviceContainer devices12 = pointToPoint.Install(nodes.Get(1), nodes.Get(2));

    // Install Internet Stack on all nodes
    ns3::InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses to Node0-Node1 segment (10.1.1.0/24)
    ns3::Ipv4AddressHelper address01;
    address01.SetBase("10.1.1.0", "255.255.255.0");
    ns3::Ipv4InterfaceContainer interfaces01 = address01.Assign(devices01);

    // Assign IP addresses to Node1-Node2 segment (10.1.2.0/24)
    ns3::Ipv4AddressHelper address12;
    address12.SetBase("10.1.2.0", "255.255.255.0");
    ns3::Ipv4InterfaceContainer interfaces12 = address12.Assign(devices12);

    // Enable global routing for traffic forwarding
    ns3::Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Configure UDP traffic from Node 0 to Node 2
    // Destination IP is Node 2's address on the 10.1.2.x segment (interfaces12.GetAddress(1))
    uint16_t port = 9; // Echo Port
    ns3::Address sinkAddress(ns3::InetSocketAddress(interfaces12.GetAddress(1), port));

    // Create and install PacketSink application on Node 2 (the destination)
    ns3::PacketSinkHelper sinkHelper("ns3::UdpSocketFactory", sinkAddress);
    ns3::ApplicationContainer sinkApps = sinkHelper.Install(nodes.Get(2));
    sinkApps.Start(ns3::Seconds(0.0));
    sinkApps.Stop(ns3::Seconds(10.0));

    // Create and install OnOff application on Node 0 (the source)
    ns3::OnOffHelper onoffHelper("ns3::UdpSocketFactory", sinkAddress);
    onoffHelper.SetAttribute("OnTime", ns3::StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoffHelper.SetAttribute("OffTime", ns3::StringValue("ns3::ConstantRandomVariable[Constant=0]")); // Always on
    onoffHelper.SetAttribute("PacketSize", ns3::UintegerValue(1024));
    onoffHelper.SetAttribute("DataRate", ns3::StringValue("1Mbps")); // 1 Mbps traffic
    
    ns3::ApplicationContainer sourceApps = onoffHelper.Install(nodes.Get(0));
    sourceApps.Start(ns3::Seconds(1.0)); // Start sending after 1 second
    sourceApps.Stop(ns3::Seconds(10.0));

    // Enable PCAP tracing on all Point-to-Point devices
    pointToPoint.EnablePcapAll("line-topology-udp");

    // Set the simulation to run for 10 seconds
    ns3::Simulator::Stop(ns3::Seconds(10.0));
    ns3::Simulator::Run();
    ns3::Simulator::Destroy();

    return 0;
}