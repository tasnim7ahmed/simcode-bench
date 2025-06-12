#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Create three nodes in a line topology
    NodeContainer nodes;
    nodes.Create(3);

    // Create point-to-point links between node 0 <-> node 1 and node 1 <-> node 2
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // Install first link (node 0 - node 1)
    NetDeviceContainer devices0_1 = p2p.Install(nodes.Get(0), nodes.Get(1));

    // Install second link (node 1 - node 2)
    NetDeviceContainer devices1_2 = p2p.Install(nodes.Get(1), nodes.Get(2));

    // Install Internet stack on all nodes
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses to both links
    Ipv4AddressHelper address0_1;
    address0_1.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces0_1 = address0_1.Assign(devices0_1);

    Ipv4AddressHelper address1_2;
    address1_2.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces1_2 = address1_2.Assign(devices1_2);

    // Set up UDP Echo Server on node 2
    uint16_t port = 9;
    UdpEchoServerHelper server(port);
    ApplicationContainer serverApp = server.Install(nodes.Get(2));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Set up UDP Echo Client on node 0 sending to node 2
    UdpEchoClientHelper client(interfaces1_2.GetAddress(1), port);
    client.SetAttribute("MaxPackets", UintegerValue(10));
    client.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    client.SetAttribute("PacketSize", UintegerValue(512));

    ApplicationContainer clientApp = client.Install(nodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Enable PCAP tracing for all devices
    p2p.EnablePcapAll("udp_line_topology");

    // Run the simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}