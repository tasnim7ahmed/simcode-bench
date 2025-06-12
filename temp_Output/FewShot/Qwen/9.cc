#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable logging
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create 4 nodes for the ring topology
    NodeContainer nodes;
    nodes.Create(4);

    // Define point-to-point link attributes
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // Connect nodes in a ring: 0-1, 1-2, 2-3, 3-0
    NetDeviceContainer devices[4];
    devices[0] = p2p.Install(nodes.Get(0), nodes.Get(1));
    devices[1] = p2p.Install(nodes.Get(1), nodes.Get(2));
    devices[2] = p2p.Install(nodes.Get(2), nodes.Get(3));
    devices[3] = p2p.Install(nodes.Get(3), nodes.Get(0));

    // Install internet stack on all nodes
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses from the base network 192.9.39.0/24
    Ipv4AddressHelper address;
    address.SetBase("192.9.39.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces[4];
    for (int i = 0; i < 4; ++i) {
        interfaces[i] = address.Assign(devices[i]);
        address.NewNetwork();
    }

    // Set up UDP Echo Server and Client applications
    uint16_t port = 9;

    // First pair: Node 0 as client, Node 1 as server
    UdpEchoServerHelper echoServer1(port);
    ApplicationContainer serverApp1 = echoServer1.Install(nodes.Get(1));
    serverApp1.Start(Seconds(1.0));
    serverApp1.Stop(Seconds(6.0));

    UdpEchoClientHelper echoClient1(interfaces[0].GetAddress(1), port);
    echoClient1.SetAttribute("MaxPackets", UintegerValue(4));
    echoClient1.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient1.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp1 = echoClient1.Install(nodes.Get(0));
    clientApp1.Start(Seconds(2.0));
    clientApp1.Stop(Seconds(6.0));

    // Second pair: Node 1 as client, Node 2 as server
    UdpEchoServerHelper echoServer2(port);
    ApplicationContainer serverApp2 = echoServer2.Install(nodes.Get(2));
    serverApp2.Start(Seconds(7.0));
    serverApp2.Stop(Seconds(12.0));

    UdpEchoClientHelper echoClient2(interfaces[1].GetAddress(1), port);
    echoClient2.SetAttribute("MaxPackets", UintegerValue(4));
    echoClient2.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient2.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp2 = echoClient2.Install(nodes.Get(1));
    clientApp2.Start(Seconds(8.0));
    clientApp2.Stop(Seconds(12.0));

    // Third pair: Node 2 as client, Node 3 as server
    UdpEchoServerHelper echoServer3(port);
    ApplicationContainer serverApp3 = echoServer3.Install(nodes.Get(3));
    serverApp3.Start(Seconds(13.0));
    serverApp3.Stop(Seconds(18.0));

    UdpEchoClientHelper echoClient3(interfaces[2].GetAddress(1), port);
    echoClient3.SetAttribute("MaxPackets", UintegerValue(4));
    echoClient3.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient3.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp3 = echoClient3.Install(nodes.Get(2));
    clientApp3.Start(Seconds(14.0));
    clientApp3.Stop(Seconds(18.0));

    // Fourth pair: Node 3 as client, Node 0 as server
    UdpEchoServerHelper echoServer4(port);
    ApplicationContainer serverApp4 = echoServer4.Install(nodes.Get(0));
    serverApp4.Start(Seconds(19.0));
    serverApp4.Stop(Seconds(24.0));

    UdpEchoClientHelper echoClient4(interfaces[3].GetAddress(1), port);
    echoClient4.SetAttribute("MaxPackets", UintegerValue(4));
    echoClient4.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient4.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp4 = echoClient4.Install(nodes.Get(3));
    clientApp4.Start(Seconds(20.0));
    clientApp4.Stop(Seconds(24.0));

    // Setup NetAnim for visualization
    AnimationInterface anim("ring-topology.xml");
    anim.SetConstantPosition(nodes.Get(0), 0.0, 0.0);
    anim.SetConstantPosition(nodes.Get(1), 10.0, 0.0);
    anim.SetConstantPosition(nodes.Get(2), 10.0, 10.0);
    anim.SetConstantPosition(nodes.Get(3), 0.0, 10.0);

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}