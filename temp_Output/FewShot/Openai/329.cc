#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // Enable logging for PacketSink and OnOffApplication
    LogComponentEnable("PacketSink", LOG_LEVEL_INFO);
    LogComponentEnable("OnOffApplication", LOG_LEVEL_INFO);

    // Create three nodes: 0 (client), 1 (client), 2 (server)
    NodeContainer nodes;
    nodes.Create(3);

    // Create point-to-point links: node0<->node2, node1<->node2
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("1Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    // Set up links and devices
    NetDeviceContainer d0d2 = pointToPoint.Install(nodes.Get(0), nodes.Get(2));
    NetDeviceContainer d1d2 = pointToPoint.Install(nodes.Get(1), nodes.Get(2));

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;

    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer i0i2 = address.Assign(d0d2);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer i1i2 = address.Assign(d1d2);

    // Install TCP server (PacketSink) on node 2, listening on port 9
    uint16_t port = 9;

    Address sinkAddress(InetSocketAddress(i0i2.GetAddress(1), port));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApp = packetSinkHelper.Install(nodes.Get(2));
    sinkApp.Start(Seconds(1.0));
    sinkApp.Stop(Seconds(10.0));

    // Set up TCP client (OnOffApplication) on node 0
    OnOffHelper client0("ns3::TcpSocketFactory", InetSocketAddress(i0i2.GetAddress(1), port));
    client0.SetAttribute("DataRate", StringValue("500Kbps"));
    client0.SetAttribute("PacketSize", UintegerValue(1024));
    client0.SetAttribute("StartTime", TimeValue(Seconds(2.0)));
    client0.SetAttribute("StopTime", TimeValue(Seconds(10.0)));
    ApplicationContainer clientApp0 = client0.Install(nodes.Get(0));

    // Set up TCP client (OnOffApplication) on node 1
    // node 1 must use server's interface address on 10.1.2.1
    OnOffHelper client1("ns3::TcpSocketFactory", InetSocketAddress(i1i2.GetAddress(1), port));
    client1.SetAttribute("DataRate", StringValue("500Kbps"));
    client1.SetAttribute("PacketSize", UintegerValue(1024));
    client1.SetAttribute("StartTime", TimeValue(Seconds(3.0)));
    client1.SetAttribute("StopTime", TimeValue(Seconds(10.0)));
    ApplicationContainer clientApp1 = client1.Install(nodes.Get(1));

    // Enable pcap tracing (optional)
    pointToPoint.EnablePcapAll("tcp-three-nodes");

    // Run simulation
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}