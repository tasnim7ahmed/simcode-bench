#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);

    // Create nodes
    NodeContainer nodes;
    nodes.Create(3);

    // Create point-to-point links: node0 <-> node1 and node0 <-> node2
    NodeContainer n0n1 = NodeContainer(nodes.Get(0), nodes.Get(1));
    NodeContainer n0n2 = NodeContainer(nodes.Get(0), nodes.Get(2));

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // Install devices
    NetDeviceContainer dev0_1 = p2p.Install(n0n1);
    NetDeviceContainer dev0_2 = p2p.Install(n0n2);

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;

    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer if0_1 = address.Assign(dev0_1);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer if0_2 = address.Assign(dev0_2);

    // Set up UDP echo server on node1 and node2
    uint16_t port1 = 9;
    UdpEchoServerHelper echoServer1(port1);
    ApplicationContainer serverApps1 = echoServer1.Install(nodes.Get(1));
    serverApps1.Start(Seconds(1.0));
    serverApps1.Stop(Seconds(10.0));

    uint16_t port2 = 10;
    UdpEchoServerHelper echoServer2(port2);
    ApplicationContainer serverApps2 = echoServer2.Install(nodes.Get(2));
    serverApps2.Start(Seconds(1.0));
    serverApps2.Stop(Seconds(10.0));

    // Set up UDP echo client on node0 targeting node1
    UdpEchoClientHelper echoClient1(if0_1.GetAddress(1), port1);
    echoClient1.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient1.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient1.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps1 = echoClient1.Install(nodes.Get(0));
    clientApps1.Start(Seconds(2.0));
    clientApps1.Stop(Seconds(9.0));

    // Set up UDP echo client on node0 targeting node2
    UdpEchoClientHelper echoClient2(if0_2.GetAddress(1), port2);
    echoClient2.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient2.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient2.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps2 = echoClient2.Install(nodes.Get(0));
    clientApps2.Start(Seconds(2.5));
    clientApps2.Stop(Seconds(9.0));

    // Enable PCAP tracing
    p2p.EnablePcapAll("branch-topology");

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}