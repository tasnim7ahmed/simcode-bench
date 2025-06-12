#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);

    // Create 4 nodes
    NodeContainer nodes;
    nodes.Create(4);

    // Create point-to-point links in a ring: 0-1-2-3-0
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer d01 = p2p.Install(nodes.Get(0), nodes.Get(1));
    NetDeviceContainer d12 = p2p.Install(nodes.Get(1), nodes.Get(2));
    NetDeviceContainer d23 = p2p.Install(nodes.Get(2), nodes.Get(3));
    NetDeviceContainer d30 = p2p.Install(nodes.Get(3), nodes.Get(0));

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    std::vector<Ipv4InterfaceContainer> interfaces(4);

    address.SetBase("192.9.39.0", "255.255.255.0");
    interfaces[0] = address.Assign(d01); // 0-1
    address.NewNetwork();
    interfaces[1] = address.Assign(d12); // 1-2
    address.NewNetwork();
    interfaces[2] = address.Assign(d23); // 2-3
    address.NewNetwork();
    interfaces[3] = address.Assign(d30); // 3-0

    // Routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // UDP Echo Port
    uint16_t port = 9;

    // Times for applications
    double startTimes[4] = {1.0, 7.0, 13.0, 19.0};
    double duration = 6.0; // Each client session lasts 4 seconds + a little margin

    // 1st communication: node 0 client -> node 1 server
    UdpEchoServerHelper echoServer1(port);
    ApplicationContainer serverApp1 = echoServer1.Install(nodes.Get(1));
    serverApp1.Start(Seconds(startTimes[0]));
    serverApp1.Stop(Seconds(startTimes[0] + duration));

    UdpEchoClientHelper echoClient1(interfaces[0].GetAddress(1), port); // node1's address on link 0-1
    echoClient1.SetAttribute("MaxPackets", UintegerValue(4));
    echoClient1.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient1.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApp1 = echoClient1.Install(nodes.Get(0));
    clientApp1.Start(Seconds(startTimes[0] + 0.5));
    clientApp1.Stop(Seconds(startTimes[0] + duration));

    // 2nd communication: node 1 client -> node 2 server
    UdpEchoServerHelper echoServer2(port);
    ApplicationContainer serverApp2 = echoServer2.Install(nodes.Get(2));
    serverApp2.Start(Seconds(startTimes[1]));
    serverApp2.Stop(Seconds(startTimes[1] + duration));

    UdpEchoClientHelper echoClient2(interfaces[1].GetAddress(1), port); // node2's address on link 1-2
    echoClient2.SetAttribute("MaxPackets", UintegerValue(4));
    echoClient2.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient2.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApp2 = echoClient2.Install(nodes.Get(1));
    clientApp2.Start(Seconds(startTimes[1] + 0.5));
    clientApp2.Stop(Seconds(startTimes[1] + duration));

    // 3rd communication: node 2 client -> node 3 server
    UdpEchoServerHelper echoServer3(port);
    ApplicationContainer serverApp3 = echoServer3.Install(nodes.Get(3));
    serverApp3.Start(Seconds(startTimes[2]));
    serverApp3.Stop(Seconds(startTimes[2] + duration));

    UdpEchoClientHelper echoClient3(interfaces[2].GetAddress(1), port); // node3's address on link 2-3
    echoClient3.SetAttribute("MaxPackets", UintegerValue(4));
    echoClient3.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient3.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApp3 = echoClient3.Install(nodes.Get(2));
    clientApp3.Start(Seconds(startTimes[2] + 0.5));
    clientApp3.Stop(Seconds(startTimes[2] + duration));

    // 4th communication: node 3 client -> node 0 server
    UdpEchoServerHelper echoServer4(port);
    ApplicationContainer serverApp4 = echoServer4.Install(nodes.Get(0));
    serverApp4.Start(Seconds(startTimes[3]));
    serverApp4.Stop(Seconds(startTimes[3] + duration));

    UdpEchoClientHelper echoClient4(interfaces[3].GetAddress(1), port); // node0's address on link 3-0
    echoClient4.SetAttribute("MaxPackets", UintegerValue(4));
    echoClient4.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient4.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApp4 = echoClient4.Install(nodes.Get(3));
    clientApp4.Start(Seconds(startTimes[3] + 0.5));
    clientApp4.Stop(Seconds(startTimes[3] + duration));

    Simulator::Stop(Seconds(26.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}