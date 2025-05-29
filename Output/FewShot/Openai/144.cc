#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int 
main(int argc, char *argv[])
{
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create 4 nodes for the ring
    NodeContainer nodes;
    nodes.Create(4);

    // Create point-to-point links for the ring (n0-n1, n1-n2, n2-n3, n3-n0)
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("1ms"));

    NetDeviceContainer d01 = p2p.Install(nodes.Get(0), nodes.Get(1));
    NetDeviceContainer d12 = p2p.Install(nodes.Get(1), nodes.Get(2));
    NetDeviceContainer d23 = p2p.Install(nodes.Get(2), nodes.Get(3));
    NetDeviceContainer d30 = p2p.Install(nodes.Get(3), nodes.Get(0));

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses to each point-to-point link
    Ipv4AddressHelper address;
    Ipv4InterfaceContainer i01, i12, i23, i30;

    address.SetBase("10.1.1.0", "255.255.255.0");
    i01 = address.Assign(d01);

    address.SetBase("10.1.2.0", "255.255.255.0");
    i12 = address.Assign(d12);

    address.SetBase("10.1.3.0", "255.255.255.0");
    i23 = address.Assign(d23);

    address.SetBase("10.1.4.0", "255.255.255.0");
    i30 = address.Assign(d30);

    // Each node runs a UDP Echo Server (for its neighbor)
    uint16_t port = 9;

    UdpEchoServerHelper echoServer01(port); // for n1
    UdpEchoServerHelper echoServer12(port); // for n2
    UdpEchoServerHelper echoServer23(port); // for n3
    UdpEchoServerHelper echoServer30(port); // for n0

    ApplicationContainer serverApps;
    serverApps.Add(echoServer01.Install(nodes.Get(1)));
    serverApps.Add(echoServer12.Install(nodes.Get(2)));
    serverApps.Add(echoServer23.Install(nodes.Get(3)));
    serverApps.Add(echoServer30.Install(nodes.Get(0)));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // For circular traffic, each node sends to its clockwise neighbor:
    // n0->n1, n1->n2, n2->n3, n3->n0

    UdpEchoClientHelper echoClient01(i01.GetAddress(1), port); // n0->n1
    echoClient01.SetAttribute("MaxPackets", UintegerValue(3));
    echoClient01.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient01.SetAttribute("PacketSize", UintegerValue(512));

    UdpEchoClientHelper echoClient12(i12.GetAddress(1), port); // n1->n2
    echoClient12.SetAttribute("MaxPackets", UintegerValue(3));
    echoClient12.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient12.SetAttribute("PacketSize", UintegerValue(512));

    UdpEchoClientHelper echoClient23(i23.GetAddress(1), port); // n2->n3
    echoClient23.SetAttribute("MaxPackets", UintegerValue(3));
    echoClient23.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient23.SetAttribute("PacketSize", UintegerValue(512));

    UdpEchoClientHelper echoClient30(i30.GetAddress(1), port); // n3->n0
    echoClient30.SetAttribute("MaxPackets", UintegerValue(3));
    echoClient30.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient30.SetAttribute("PacketSize", UintegerValue(512));

    ApplicationContainer clientApps;
    clientApps.Add(echoClient01.Install(nodes.Get(0)));
    clientApps.Add(echoClient12.Install(nodes.Get(1)));
    clientApps.Add(echoClient23.Install(nodes.Get(2)));
    clientApps.Add(echoClient30.Install(nodes.Get(3)));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));
    
    // Enable routing for multi-hop communication if needed in more complex rings
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}