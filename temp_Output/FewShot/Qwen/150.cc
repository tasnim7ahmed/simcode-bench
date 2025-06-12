#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Create 4 nodes in a line topology
    NodeContainer nodes;
    nodes.Create(4);

    // Create point-to-point links between node0-node1, node1-node2, and node2-node3
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices01 = p2p.Install(nodes.Get(0), nodes.Get(1));
    NetDeviceContainer devices12 = p2p.Install(nodes.Get(1), nodes.Get(2));
    NetDeviceContainer devices23 = p2p.Install(nodes.Get(2), nodes.Get(3));

    // Install internet stack on all nodes
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses to each network segment
    Ipv4AddressHelper address01;
    address01.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces01 = address01.Assign(devices01);

    Ipv4AddressHelper address12;
    address12.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces12 = address12.Assign(devices12);

    Ipv4AddressHelper address23;
    address23.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces23 = address23.Assign(devices23);

    // Set up UDP echo server on node 3
    uint16_t port = 9;
    UdpEchoServerHelper server(port);
    ApplicationContainer serverApp = server.Install(nodes.Get(3));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Set up UDP echo client on node 0 sending to node 3
    UdpEchoClientHelper client(interfaces23.GetAddress(1), port);
    client.SetAttribute("MaxPackets", UintegerValue(10));
    client.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    client.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp = client.Install(nodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Enable PCAP tracing for all devices
    p2p.EnablePcapAll("line_topology_udp");

    // Run the simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}