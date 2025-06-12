#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    NodeContainer nodes;
    nodes.Create(4);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices;
    for (int i = 0; i < 4; ++i) {
        NetDeviceContainer link;
        if (i < 3) {
            link = pointToPoint.Install(nodes.Get(i), nodes.Get(i + 1));
        } else {
            link = pointToPoint.Install(nodes.Get(i), nodes.Get(0));
        }
        devices.Add(link.Get(0));
        devices.Add(link.Get(1));
    }

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("192.9.39.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    uint16_t port = 9;

    // Node 0 as client, Node 1 as server
    UdpEchoServerHelper echoServer01(port);
    ApplicationContainer serverApp01 = echoServer01.Install(nodes.Get(1));
    serverApp01.Start(Seconds(1.0));
    serverApp01.Stop(Seconds(5.0));

    UdpEchoClientHelper echoClient01(interfaces.GetAddress(3), port);
    echoClient01.SetAttribute("MaxPackets", UintegerValue(4));
    echoClient01.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient01.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp01 = echoClient01.Install(nodes.Get(0));
    clientApp01.Start(Seconds(2.0));
    clientApp01.Stop(Seconds(5.0));

    // Node 1 as client, Node 2 as server
    UdpEchoServerHelper echoServer12(port);
    ApplicationContainer serverApp12 = echoServer12.Install(nodes.Get(2));
    serverApp12.Start(Seconds(6.0));
    serverApp12.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient12(interfaces.GetAddress(5), port);
    echoClient12.SetAttribute("MaxPackets", UintegerValue(4));
    echoClient12.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient12.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp12 = echoClient12.Install(nodes.Get(1));
    clientApp12.Start(Seconds(7.0));
    clientApp12.Stop(Seconds(10.0));

    // Node 2 as client, Node 3 as server
    UdpEchoServerHelper echoServer23(port);
    ApplicationContainer serverApp23 = echoServer23.Install(nodes.Get(3));
    serverApp23.Start(Seconds(11.0));
    serverApp23.Stop(Seconds(15.0));

    UdpEchoClientHelper echoClient23(interfaces.GetAddress(7), port);
    echoClient23.SetAttribute("MaxPackets", UintegerValue(4));
    echoClient23.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient23.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp23 = echoClient23.Install(nodes.Get(2));
    clientApp23.Start(Seconds(12.0));
    clientApp23.Stop(Seconds(15.0));

     // Node 3 as client, Node 0 as server
    UdpEchoServerHelper echoServer30(port);
    ApplicationContainer serverApp30 = echoServer30.Install(nodes.Get(0));
    serverApp30.Start(Seconds(16.0));
    serverApp30.Stop(Seconds(20.0));

    UdpEchoClientHelper echoClient30(interfaces.GetAddress(1), port);
    echoClient30.SetAttribute("MaxPackets", UintegerValue(4));
    echoClient30.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient30.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp30 = echoClient30.Install(nodes.Get(3));
    clientApp30.Start(Seconds(17.0));
    clientApp30.Stop(Seconds(20.0));

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}