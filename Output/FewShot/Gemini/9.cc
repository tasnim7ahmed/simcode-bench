#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/animation-interface.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    NodeContainer nodes;
    nodes.Create(4);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices[4];
    devices[0] = pointToPoint.Install(nodes.Get(0), nodes.Get(1));
    devices[1] = pointToPoint.Install(nodes.Get(1), nodes.Get(2));
    devices[2] = pointToPoint.Install(nodes.Get(2), nodes.Get(3));
    devices[3] = pointToPoint.Install(nodes.Get(3), nodes.Get(0));

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("192.9.39.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces[4];
    interfaces[0] = address.Assign(devices[0]);
    interfaces[1] = address.Assign(devices[1]);
    interfaces[2] = address.Assign(devices[2]);
    interfaces[3] = address.Assign(devices[3]);

    uint16_t port = 9;

    // Node 0 as client, Node 1 as server
    UdpEchoServerHelper echoServer0(port);
    ApplicationContainer serverApp0 = echoServer0.Install(nodes.Get(1));
    serverApp0.Start(Seconds(1.0));
    serverApp0.Stop(Seconds(5.0));

    UdpEchoClientHelper echoClient0(interfaces[0].GetAddress(1), port);
    echoClient0.SetAttribute("MaxPackets", UintegerValue(4));
    echoClient0.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient0.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApp0 = echoClient0.Install(nodes.Get(0));
    clientApp0.Start(Seconds(2.0));
    clientApp0.Stop(Seconds(5.0));

    // Node 1 as client, Node 2 as server
    UdpEchoServerHelper echoServer1(port);
    ApplicationContainer serverApp1 = echoServer1.Install(nodes.Get(2));
    serverApp1.Start(Seconds(6.0));
    serverApp1.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient1(interfaces[1].GetAddress(1), port);
    echoClient1.SetAttribute("MaxPackets", UintegerValue(4));
    echoClient1.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient1.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApp1 = echoClient1.Install(nodes.Get(1));
    clientApp1.Start(Seconds(7.0));
    clientApp1.Stop(Seconds(10.0));

    // Node 2 as client, Node 3 as server
     UdpEchoServerHelper echoServer2(port);
    ApplicationContainer serverApp2 = echoServer2.Install(nodes.Get(3));
    serverApp2.Start(Seconds(11.0));
    serverApp2.Stop(Seconds(15.0));

    UdpEchoClientHelper echoClient2(interfaces[2].GetAddress(1), port);
    echoClient2.SetAttribute("MaxPackets", UintegerValue(4));
    echoClient2.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient2.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApp2 = echoClient2.Install(nodes.Get(2));
    clientApp2.Start(Seconds(12.0));
    clientApp2.Stop(Seconds(15.0));


     // Node 3 as client, Node 0 as server
     UdpEchoServerHelper echoServer3(port);
    ApplicationContainer serverApp3 = echoServer3.Install(nodes.Get(0));
    serverApp3.Start(Seconds(16.0));
    serverApp3.Stop(Seconds(20.0));

    UdpEchoClientHelper echoClient3(interfaces[3].GetAddress(1), port);
    echoClient3.SetAttribute("MaxPackets", UintegerValue(4));
    echoClient3.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient3.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApp3 = echoClient3.Install(nodes.Get(3));
    clientApp3.Start(Seconds(17.0));
    clientApp3.Stop(Seconds(20.0));

    AnimationInterface anim("ring_network.xml");
    anim.SetConstantPosition(nodes.Get(0), 1, 1);
    anim.SetConstantPosition(nodes.Get(1), 3, 1);
    anim.SetConstantPosition(nodes.Get(2), 5, 1);
    anim.SetConstantPosition(nodes.Get(3), 7, 1);

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}