#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Create 3 nodes in a line
    NodeContainer nodes;
    nodes.Create(3);

    // Point-to-point link helpers
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // Link: Node 0 <-> Node 1
    NodeContainer n0n1 = NodeContainer(nodes.Get(0), nodes.Get(1));
    NetDeviceContainer d0d1 = p2p.Install(n0n1);

    // Link: Node 1 <-> Node 2
    NodeContainer n1n2 = NodeContainer(nodes.Get(1), nodes.Get(2));
    NetDeviceContainer d1d2 = p2p.Install(n1n2);

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper addr;
    addr.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer i0i1 = addr.Assign(d0d1);

    addr.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer i1i2 = addr.Assign(d1d2);

    // Enable PCAP tracing
    p2p.EnablePcapAll("line-topology");

    // Set up UDP server (sink) on node 2
    uint16_t port = 4000;
    UdpEchoServerHelper server(port);
    ApplicationContainer serverApp = server.Install(nodes.Get(2));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Set up UDP client on node 0 targeting node 2
    UdpEchoClientHelper client(i1i2.GetAddress(1), port); // Node 2's address
    client.SetAttribute("MaxPackets", UintegerValue(10));
    client.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    client.SetAttribute("PacketSize", UintegerValue(512));
    ApplicationContainer clientApp = client.Install(nodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Set up routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}