#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int
main(int argc, char *argv[])
{
    // Set time resolution and logging
    Time::SetResolution(Time::NS);

    // Create 3 nodes
    NodeContainer nodes;
    nodes.Create(3);

    // Create two point-to-point links:
    // Node 0 <-> Node 2
    // Node 1 <-> Node 2
    NodeContainer n0n2 = NodeContainer(nodes.Get(0), nodes.Get(2));
    NodeContainer n1n2 = NodeContainer(nodes.Get(1), nodes.Get(2));

    // Configure point-to-point helper
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("1Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // Install links
    NetDeviceContainer d0d2 = p2p.Install(n0n2);
    NetDeviceContainer d1d2 = p2p.Install(n1n2);

    // Install the Internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer i0i2 = address.Assign(d0d2);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer i1i2 = address.Assign(d1d2);

    // Install TCP server on Node 2 listening on port 9
    uint16_t port = 9;
    Address serverAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", serverAddress);
    ApplicationContainer serverApp = packetSinkHelper.Install(nodes.Get(2)); // Node 2
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // TCP client 1 (Node 0) connects to the interface of Node 2 on network 10.1.1.0
    OnOffHelper client1("ns3::TcpSocketFactory",
                        Address(InetSocketAddress(i0i2.GetAddress(1), port)));
    client1.SetAttribute("DataRate", StringValue("500Kbps"));
    client1.SetAttribute("PacketSize", UintegerValue(1024));
    client1.SetAttribute("StartTime", TimeValue(Seconds(2.0)));
    client1.SetAttribute("StopTime", TimeValue(Seconds(10.0)));
    ApplicationContainer clientApp1 = client1.Install(nodes.Get(0));

    // TCP client 2 (Node 1) connects to the interface of Node 2 on network 10.1.2.0
    OnOffHelper client2("ns3::TcpSocketFactory",
                        Address(InetSocketAddress(i1i2.GetAddress(1), port)));
    client2.SetAttribute("DataRate", StringValue("500Kbps"));
    client2.SetAttribute("PacketSize", UintegerValue(1024));
    client2.SetAttribute("StartTime", TimeValue(Seconds(3.0)));
    client2.SetAttribute("StopTime", TimeValue(Seconds(10.0)));
    ApplicationContainer clientApp2 = client2.Install(nodes.Get(1));

    // Enable routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}