#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/packet-sink.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable logging for packet sink and UDP client/server
    LogComponentEnable("PacketSink", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create three nodes (0: central, 1 and 2: branches)
    NodeContainer nodes;
    nodes.Create(3);

    // Central node (node 0) connected to node 1 and node 2
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // Install point-to-point links between node 0-1 and node 0-2
    NetDeviceContainer devices01 = p2p.Install(nodes.Get(0), nodes.Get(1));
    NetDeviceContainer devices02 = p2p.Install(nodes.Get(0), nodes.Get(2));

    // Install Internet stack on all nodes
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address01;
    address01.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces01 = address01.Assign(devices01);

    Ipv4AddressHelper address02;
    address02.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces02 = address02.Assign(devices02);

    // Set up UDP Echo Server on node 1
    uint16_t port1 = 9;
    UdpEchoServerHelper server1(port1);
    ApplicationContainer serverApp1 = server1.Install(nodes.Get(1));
    serverApp1.Start(Seconds(1.0));
    serverApp1.Stop(Seconds(10.0));

    // Set up UDP Echo Client on node 2 sending to node 1
    UdpEchoClientHelper client1(interfaces01.GetAddress(1), port1);
    client1.SetAttribute("MaxPackets", UintegerValue(5));
    client1.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    client1.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp1 = client1.Install(nodes.Get(2));
    clientApp1.Start(Seconds(2.0));
    clientApp1.Stop(Seconds(10.0));

    // Set up UDP Echo Server on node 2
    uint16_t port2 = 10;
    UdpEchoServerHelper server2(port2);
    ApplicationContainer serverApp2 = server2.Install(nodes.Get(2));
    serverApp2.Start(Seconds(1.0));
    serverApp2.Stop(Seconds(10.0));

    // Set up UDP Echo Client on node 1 sending to node 2
    UdpEchoClientHelper client2(interfaces02.GetAddress(1), port2);
    client2.SetAttribute("MaxPackets", UintegerValue(5));
    client2.SetAttribute("Interval", TimeValue(Seconds(1.5)));
    client2.SetAttribute("PacketSize", UintegerValue(512));

    ApplicationContainer clientApp2 = client2.Install(nodes.Get(1));
    clientApp2.Start(Seconds(2.5));
    clientApp2.Stop(Seconds(10.0));

    // Enable PCAP tracing for traffic analysis
    p2p.EnablePcapAll("branch_topology_udp");

    // Run the simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}