#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    CommandLine cmd;
    cmd.Parse(argc, argv);

    NodeContainer nodes;
    nodes.Create(4);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // Central node is node 0
    // Link 0: nodes 0-1
    NodeContainer n0n1 = NodeContainer(nodes.Get(0), nodes.Get(1));
    NetDeviceContainer d0d1 = p2p.Install(n0n1);

    // Link 1: nodes 0-2
    NodeContainer n0n2 = NodeContainer(nodes.Get(0), nodes.Get(2));
    NetDeviceContainer d0d2 = p2p.Install(n0n2);

    // Link 2: nodes 0-3
    NodeContainer n0n3 = NodeContainer(nodes.Get(0), nodes.Get(3));
    NetDeviceContainer d0d3 = p2p.Install(n0n3);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;

    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer i0i1 = address.Assign(d0d1);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer i0i2 = address.Assign(d0d2);

    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer i0i3 = address.Assign(d0d3);

    // UDP server on node 0
    uint16_t port1 = 8001;
    uint16_t port2 = 8002;
    uint16_t port3 = 8003;
    UdpServerHelper server1(port1);
    UdpServerHelper server2(port2);
    UdpServerHelper server3(port3);
    ApplicationContainer serverApp1 = server1.Install(nodes.Get(0));
    ApplicationContainer serverApp2 = server2.Install(nodes.Get(0));
    ApplicationContainer serverApp3 = server3.Install(nodes.Get(0));

    serverApp1.Start(Seconds(0.0));
    serverApp1.Stop(Seconds(10.0));
    serverApp2.Start(Seconds(0.0));
    serverApp2.Stop(Seconds(10.0));
    serverApp3.Start(Seconds(0.0));
    serverApp3.Stop(Seconds(10.0));

    // UDP Client from node 1 -> node 0
    UdpClientHelper client1(i0i1.GetAddress(0), port1);
    client1.SetAttribute("MaxPackets", UintegerValue(1000));
    client1.SetAttribute("Interval", TimeValue(MilliSeconds(20)));
    client1.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApp1 = client1.Install(nodes.Get(1));

    // UDP Client from node 2 -> node 0
    UdpClientHelper client2(i0i2.GetAddress(0), port2);
    client2.SetAttribute("MaxPackets", UintegerValue(1000));
    client2.SetAttribute("Interval", TimeValue(MilliSeconds(20)));
    client2.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApp2 = client2.Install(nodes.Get(2));

    // UDP Client from node 3 -> node 0
    UdpClientHelper client3(i0i3.GetAddress(0), port3);
    client3.SetAttribute("MaxPackets", UintegerValue(1000));
    client3.SetAttribute("Interval", TimeValue(MilliSeconds(20)));
    client3.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApp3 = client3.Install(nodes.Get(3));

    clientApp1.Start(Seconds(1.0));
    clientApp1.Stop(Seconds(10.0));
    clientApp2.Start(Seconds(1.0));
    clientApp2.Stop(Seconds(10.0));
    clientApp3.Start(Seconds(1.0));
    clientApp3.Stop(Seconds(10.0));

    p2p.EnablePcapAll("branch-topology");

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}