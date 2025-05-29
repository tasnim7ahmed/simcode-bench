#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // Enable logging (optional)
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create 4 nodes: node 0 is central, nodes 1,2,3 are branch nodes
    NodeContainer nodes;
    nodes.Create(4);

    // Create 3 point-to-point links: 0-1, 0-2, 0-3
    NodeContainer n0n1 = NodeContainer(nodes.Get(0), nodes.Get(1));
    NodeContainer n0n2 = NodeContainer(nodes.Get(0), nodes.Get(2));
    NodeContainer n0n3 = NodeContainer(nodes.Get(0), nodes.Get(3));

    // Point-to-point helper with default parameters
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer d0d1 = p2p.Install(n0n1);
    NetDeviceContainer d0d2 = p2p.Install(n0n2);
    NetDeviceContainer d0d3 = p2p.Install(n0n3);

    // Install the Internet stack
    InternetStackHelper internet;
    internet.Install(nodes);

    // Assign IP addresses to each segment
    Ipv4AddressHelper address;

    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer i0i1 = address.Assign(d0d1);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer i0i2 = address.Assign(d0d2);

    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer i0i3 = address.Assign(d0d3);

    // Install UDP Echo Servers on node 0 (one server per branch for unique ports)
    uint16_t port1 = 10001;
    uint16_t port2 = 10002;
    uint16_t port3 = 10003;

    UdpEchoServerHelper server1(port1);
    ApplicationContainer appServer1 = server1.Install(nodes.Get(0));
    appServer1.Start(Seconds(0.1));
    appServer1.Stop(Seconds(10.0));

    UdpEchoServerHelper server2(port2);
    ApplicationContainer appServer2 = server2.Install(nodes.Get(0));
    appServer2.Start(Seconds(0.1));
    appServer2.Stop(Seconds(10.0));

    UdpEchoServerHelper server3(port3);
    ApplicationContainer appServer3 = server3.Install(nodes.Get(0));
    appServer3.Start(Seconds(0.1));
    appServer3.Stop(Seconds(10.0));

    // Install UDP Echo Clients on nodes 1,2,3 to send to node 0's appropriate interface and port
    UdpEchoClientHelper client1(i0i1.GetAddress(0), port1); // node 1 to node 0
    client1.SetAttribute("MaxPackets", UintegerValue(10));
    client1.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    client1.SetAttribute("PacketSize", UintegerValue(512));
    ApplicationContainer appClient1 = client1.Install(nodes.Get(1));
    appClient1.Start(Seconds(1.0));
    appClient1.Stop(Seconds(10.0));

    UdpEchoClientHelper client2(i0i2.GetAddress(0), port2); // node 2 to node 0
    client2.SetAttribute("MaxPackets", UintegerValue(10));
    client2.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    client2.SetAttribute("PacketSize", UintegerValue(512));
    ApplicationContainer appClient2 = client2.Install(nodes.Get(2));
    appClient2.Start(Seconds(1.0));
    appClient2.Stop(Seconds(10.0));

    UdpEchoClientHelper client3(i0i3.GetAddress(0), port3); // node 3 to node 0
    client3.SetAttribute("MaxPackets", UintegerValue(10));
    client3.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    client3.SetAttribute("PacketSize", UintegerValue(512));
    ApplicationContainer appClient3 = client3.Install(nodes.Get(3));
    appClient3.Start(Seconds(1.0));
    appClient3.Stop(Seconds(10.0));

    // Enable PCAP tracing on all point-to-point devices
    p2p.EnablePcapAll("branch-topology");

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}