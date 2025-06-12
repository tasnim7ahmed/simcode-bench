#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    LogComponentEnable("OnOffApplication", LOG_LEVEL_INFO);
    LogComponentEnable("PacketSink", LOG_LEVEL_INFO);

    NodeContainer nodes;
    nodes.Create(3);

    // Create and configure point-to-point links for the ring
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    // Link: Node 0 <-> Node 1
    NodeContainer n0n1 = NodeContainer(nodes.Get(0), nodes.Get(1));
    NetDeviceContainer d0d1 = pointToPoint.Install(n0n1);

    // Link: Node 1 <-> Node 2
    NodeContainer n1n2 = NodeContainer(nodes.Get(1), nodes.Get(2));
    NetDeviceContainer d1d2 = pointToPoint.Install(n1n2);

    // Link: Node 2 <-> Node 0 (completes the ring)
    NodeContainer n2n0 = NodeContainer(nodes.Get(2), nodes.Get(0));
    NetDeviceContainer d2d0 = pointToPoint.Install(n2n0);

    // Install the Internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses to the links
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer i0i1 = address.Assign(d0d1);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer i1i2 = address.Assign(d1d2);

    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer i2i0 = address.Assign(d2d0);

    // Install applications to simulate packet transmission in a ring

    // Node 0 sends to Node 1
    uint16_t port1 = 8001;
    Address sinkAddress1(InetSocketAddress(i0i1.GetAddress(1), port1));
    PacketSinkHelper sinkHelper1("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port1));
    ApplicationContainer sinkApp1 = sinkHelper1.Install(nodes.Get(1));
    sinkApp1.Start(Seconds(0.0));
    sinkApp1.Stop(Seconds(10.0));

    OnOffHelper onoff1("ns3::TcpSocketFactory", sinkAddress1);
    onoff1.SetAttribute("DataRate", StringValue("5Mbps"));
    onoff1.SetAttribute("PacketSize", UintegerValue(1024));
    onoff1.SetAttribute("StartTime", TimeValue(Seconds(1.0)));
    onoff1.SetAttribute("StopTime", TimeValue(Seconds(6.0)));
    ApplicationContainer app1 = onoff1.Install(nodes.Get(0));

    // Node 1 sends to Node 2
    uint16_t port2 = 8002;
    Address sinkAddress2(InetSocketAddress(i1i2.GetAddress(1), port2));
    PacketSinkHelper sinkHelper2("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port2));
    ApplicationContainer sinkApp2 = sinkHelper2.Install(nodes.Get(2));
    sinkApp2.Start(Seconds(0.0));
    sinkApp2.Stop(Seconds(10.0));

    OnOffHelper onoff2("ns3::TcpSocketFactory", sinkAddress2);
    onoff2.SetAttribute("DataRate", StringValue("5Mbps"));
    onoff2.SetAttribute("PacketSize", UintegerValue(1024));
    onoff2.SetAttribute("StartTime", TimeValue(Seconds(2.0)));
    onoff2.SetAttribute("StopTime", TimeValue(Seconds(7.0)));
    ApplicationContainer app2 = onoff2.Install(nodes.Get(1));

    // Node 2 sends to Node 0
    uint16_t port3 = 8003;
    Address sinkAddress3(InetSocketAddress(i2i0.GetAddress(1), port3));
    PacketSinkHelper sinkHelper3("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port3));
    ApplicationContainer sinkApp3 = sinkHelper3.Install(nodes.Get(0));
    sinkApp3.Start(Seconds(0.0));
    sinkApp3.Stop(Seconds(10.0));

    OnOffHelper onoff3("ns3::TcpSocketFactory", sinkAddress3);
    onoff3.SetAttribute("DataRate", StringValue("5Mbps"));
    onoff3.SetAttribute("PacketSize", UintegerValue(1024));
    onoff3.SetAttribute("StartTime", TimeValue(Seconds(3.0)));
    onoff3.SetAttribute("StopTime", TimeValue(Seconds(8.0)));
    ApplicationContainer app3 = onoff3.Install(nodes.Get(2));

    // Enable routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Run the simulation
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}