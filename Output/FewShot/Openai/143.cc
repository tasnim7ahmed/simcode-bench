#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("TreeTopologySimulation");

int main(int argc, char *argv[]) {
    LogComponentEnable("OnOffApplication", LOG_LEVEL_INFO);
    LogComponentEnable("PacketSink", LOG_LEVEL_INFO);

    // Create 4 nodes: n0 (root), n1 (child), n2 (child), n3 (leaf)
    NodeContainer nodes;
    nodes.Create(4);

    // Create the tree topology:
    // n0 -- n1
    //  |      
    // n2
    // n1 -- n3

    // Create links
    // Root n0 <--> n1
    // Root n0 <--> n2
    // n1 <--> n3
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // Connections
    NetDeviceContainer d0n1 = p2p.Install(nodes.Get(0), nodes.Get(1)); // n0-n1
    NetDeviceContainer d0n2 = p2p.Install(nodes.Get(0), nodes.Get(2)); // n0-n2
    NetDeviceContainer d1n3 = p2p.Install(nodes.Get(1), nodes.Get(3)); // n1-n3

    // Install network stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign addresses
    Ipv4AddressHelper address;

    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer i0n1 = address.Assign(d0n1);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer i0n2 = address.Assign(d0n2);

    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer i1n3 = address.Assign(d1n3);

    // Enable routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Application settings
    uint16_t sinkPort = 8080;

    // Install PacketSink (receiver) on root node n0
    Address sinkAddress (InetSocketAddress(i0n1.GetAddress(0), sinkPort));
    PacketSinkHelper packetSinkHelper ("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
    ApplicationContainer sinkApp = packetSinkHelper.Install(nodes.Get(0));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(10.0));

    // Install OnOffApplication (sender) on leaf node n3, destination is root n0
    OnOffHelper onoff ("ns3::TcpSocketFactory", sinkAddress);
    onoff.SetAttribute("DataRate", StringValue("1Mbps"));
    onoff.SetAttribute("PacketSize", UintegerValue(1024));
    onoff.SetAttribute("StartTime", TimeValue(Seconds(1.0)));
    onoff.SetAttribute("StopTime", TimeValue(Seconds(9.0)));
    ApplicationContainer senderApp = onoff.Install(nodes.Get(3));

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}