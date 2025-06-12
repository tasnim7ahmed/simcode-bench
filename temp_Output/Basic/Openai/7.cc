#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int
main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);

    NodeContainer nodes;
    nodes.Create(4);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices[4];
    Ipv4InterfaceContainer interfaces[4];

    // Connect node0<->node1, node1<->node2, node2<->node3, node3<->node0 (ring)
    devices[0] = p2p.Install(nodes.Get(0), nodes.Get(1));
    devices[1] = p2p.Install(nodes.Get(1), nodes.Get(2));
    devices[2] = p2p.Install(nodes.Get(2), nodes.Get(3));
    devices[3] = p2p.Install(nodes.Get(3), nodes.Get(0));

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("192.9.39.0", "255.255.255.0");

    interfaces[0] = address.Assign(devices[0]);
    address.NewNetwork(); // But using 192.9.39.0/24 for all, so manually increment
    interfaces[1] = address.Assign(devices[1]);
    address.NewNetwork();
    interfaces[2] = address.Assign(devices[2]);
    address.NewNetwork();
    interfaces[3] = address.Assign(devices[3]);

    // To make sure each node has an unique address, we get first/second address for each link
    // But all on 192.9.39.0/24; that's fine for this case.

    uint16_t port = 9;

    // Schedule client-server pairs: node0->node1, node1->node2, node2->node3, node3->node0
    // Only one pair runs at any time, for 4 seconds each

    ApplicationContainer serverApps, clientApps;

    // node0 client, node1 server: run 0~4s
    UdpEchoServerHelper echoServer0(port);
    serverApps.Add(echoServer0.Install(nodes.Get(1)));
    UdpEchoClientHelper echoClient0(interfaces[0].GetAddress(1), port);
    echoClient0.SetAttribute("MaxPackets", UintegerValue(4));
    echoClient0.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient0.SetAttribute("PacketSize", UintegerValue(1024));
    clientApps.Add(echoClient0.Install(nodes.Get(0)));
    serverApps.Get(0)->SetStartTime(Seconds(0.0));
    serverApps.Get(0)->SetStopTime(Seconds(4.0));
    clientApps.Get(0)->SetStartTime(Seconds(0.1));
    clientApps.Get(0)->SetStopTime(Seconds(4.0));

    // node1 client, node2 server: run 4~8s
    UdpEchoServerHelper echoServer1(port);
    serverApps.Add(echoServer1.Install(nodes.Get(2)));
    UdpEchoClientHelper echoClient1(interfaces[1].GetAddress(1), port);
    echoClient1.SetAttribute("MaxPackets", UintegerValue(4));
    echoClient1.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient1.SetAttribute("PacketSize", UintegerValue(1024));
    clientApps.Add(echoClient1.Install(nodes.Get(1)));
    serverApps.Get(1)->SetStartTime(Seconds(4.0));
    serverApps.Get(1)->SetStopTime(Seconds(8.0));
    clientApps.Get(1)->SetStartTime(Seconds(4.1));
    clientApps.Get(1)->SetStopTime(Seconds(8.0));

    // node2 client, node3 server: run 8~12s
    UdpEchoServerHelper echoServer2(port);
    serverApps.Add(echoServer2.Install(nodes.Get(3)));
    UdpEchoClientHelper echoClient2(interfaces[2].GetAddress(1), port);
    echoClient2.SetAttribute("MaxPackets", UintegerValue(4));
    echoClient2.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient2.SetAttribute("PacketSize", UintegerValue(1024));
    clientApps.Add(echoClient2.Install(nodes.Get(2)));
    serverApps.Get(2)->SetStartTime(Seconds(8.0));
    serverApps.Get(2)->SetStopTime(Seconds(12.0));
    clientApps.Get(2)->SetStartTime(Seconds(8.1));
    clientApps.Get(2)->SetStopTime(Seconds(12.0));

    // node3 client, node0 server: run 12~16s
    UdpEchoServerHelper echoServer3(port);
    serverApps.Add(echoServer3.Install(nodes.Get(0)));
    UdpEchoClientHelper echoClient3(interfaces[3].GetAddress(1), port);
    echoClient3.SetAttribute("MaxPackets", UintegerValue(4));
    echoClient3.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient3.SetAttribute("PacketSize", UintegerValue(1024));
    clientApps.Add(echoClient3.Install(nodes.Get(3)));
    serverApps.Get(3)->SetStartTime(Seconds(12.0));
    serverApps.Get(3)->SetStopTime(Seconds(16.0));
    clientApps.Get(3)->SetStartTime(Seconds(12.1));
    clientApps.Get(3)->SetStopTime(Seconds(16.0));

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    Simulator::Stop(Seconds(17.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}