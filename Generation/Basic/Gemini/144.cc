#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    NodeContainer nodes;
    nodes.Create(4);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("1ms"));

    NetDeviceContainer d0d1 = p2p.Install(NodeContainer(nodes.Get(0), nodes.Get(1)));
    NetDeviceContainer d1d2 = p2p.Install(NodeContainer(nodes.Get(1), nodes.Get(2)));
    NetDeviceContainer d2d3 = p2p.Install(NodeContainer(nodes.Get(2), nodes.Get(3)));
    NetDeviceContainer d3d0 = p2p.Install(NodeContainer(nodes.Get(3), nodes.Get(0)));

    InternetStackHelper internet;
    internet.Install(nodes);

    Ipv4AddressHelper ipv4;

    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer i0i1 = ipv4.Assign(d0d1);

    ipv4.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer i1i2 = ipv4.Assign(d1d2);

    ipv4.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer i2i3 = ipv4.Assign(d2d3);

    ipv4.SetBase("10.1.4.0", "255.255.255.0");
    Ipv4InterfaceContainer i3i0 = ipv4.Assign(d3d0);

    uint16_t port = 9;

    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApps = echoServer.Install(nodes);
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(11.0));

    // Node 0 sends to Node 1's IP on link 0-1 (i.e., 10.1.1.2)
    UdpEchoClientHelper echoClient0(i0i1.GetAddress(1), port);
    echoClient0.SetAttribute("MaxPackets", UintegerValue(10));
    echoClient0.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient0.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApp0 = echoClient0.Install(nodes.Get(0));
    clientApp0.Start(Seconds(1.0));
    clientApp0.Stop(Seconds(10.0));

    // Node 1 sends to Node 2's IP on link 1-2 (i.e., 10.1.2.2)
    UdpEchoClientHelper echoClient1(i1i2.GetAddress(1), port);
    echoClient1.SetAttribute("MaxPackets", UintegerValue(10));
    echoClient1.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient1.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApp1 = echoClient1.Install(nodes.Get(1));
    clientApp1.Start(Seconds(1.0));
    clientApp1.Stop(Seconds(10.0));

    // Node 2 sends to Node 3's IP on link 2-3 (i.e., 10.1.3.2)
    UdpEchoClientHelper echoClient2(i2i3.GetAddress(1), port);
    echoClient2.SetAttribute("MaxPackets", UintegerValue(10));
    echoClient2.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient2.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApp2 = echoClient2.Install(nodes.Get(2));
    clientApp2.Start(Seconds(1.0));
    clientApp2.Stop(Seconds(10.0));

    // Node 3 sends to Node 0's IP on link 3-0 (i.e., 10.1.4.2)
    UdpEchoClientHelper echoClient3(i3i0.GetAddress(1), port);
    echoClient3.SetAttribute("MaxPackets", UintegerValue(10));
    echoClient3.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient3.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApp3 = echoClient3.Install(nodes.Get(3));
    clientApp3.Start(Seconds(1.0));
    clientApp3.Stop(Seconds(10.0));

    Simulator::Stop(Seconds(11.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}