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
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer d01 = p2p.Install(nodes.Get(0), nodes.Get(1));
    NetDeviceContainer d12 = p2p.Install(nodes.Get(1), nodes.Get(2));
    NetDeviceContainer d23 = p2p.Install(nodes.Get(2), nodes.Get(3));
    NetDeviceContainer d30 = p2p.Install(nodes.Get(3), nodes.Get(0));

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    Ipv4InterfaceContainer ifaces01, ifaces12, ifaces23, ifaces30;

    address.SetBase("192.9.39.0", "255.255.255.0");
    ifaces01 = address.Assign(d01);
    address.SetBase("192.9.39.4", "255.255.255.0");
    ifaces12 = address.Assign(d12);
    address.SetBase("192.9.39.8", "255.255.255.0");
    ifaces23 = address.Assign(d23);
    address.SetBase("192.9.39.12", "255.255.255.0");
    ifaces30 = address.Assign(d30);

    uint16_t port = 9;

    ApplicationContainer serverApps, clientApps;

    // 1st pair: Node 0 client -> Node 2 server
    UdpEchoServerHelper server1(port);
    serverApps = server1.Install(nodes.Get(2));
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(5.0));

    UdpEchoClientHelper client1(Ipv4Address("192.9.39.10"), port);
    client1.SetAttribute("MaxPackets", UintegerValue(4));
    client1.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    client1.SetAttribute("PacketSize", UintegerValue(1024));
    clientApps = client1.Install(nodes.Get(0));
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(5.0));

    // 2nd pair: Node 1 client -> Node 3 server
    UdpEchoServerHelper server2(port);
    serverApps = server2.Install(nodes.Get(3));
    serverApps.Start(Seconds(5.1));
    serverApps.Stop(Seconds(10.0));

    UdpEchoClientHelper client2(Ipv4Address("192.9.39.13"), port);
    client2.SetAttribute("MaxPackets", UintegerValue(4));
    client2.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    client2.SetAttribute("PacketSize", UintegerValue(1024));
    clientApps = client2.Install(nodes.Get(1));
    clientApps.Start(Seconds(6.0));
    clientApps.Stop(Seconds(10.0));

    // 3rd pair: Node 2 client -> Node 0 server
    UdpEchoServerHelper server3(port);
    serverApps = server3.Install(nodes.Get(0));
    serverApps.Start(Seconds(10.1));
    serverApps.Stop(Seconds(15.0));

    UdpEchoClientHelper client3(Ipv4Address("192.9.39.1"), port);
    client3.SetAttribute("MaxPackets", UintegerValue(4));
    client3.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    client3.SetAttribute("PacketSize", UintegerValue(1024));
    clientApps = client3.Install(nodes.Get(2));
    clientApps.Start(Seconds(11.0));
    clientApps.Stop(Seconds(15.0));

    // 4th pair: Node 3 client -> Node 1 server
    UdpEchoServerHelper server4(port);
    serverApps = server4.Install(nodes.Get(1));
    serverApps.Start(Seconds(15.1));
    serverApps.Stop(Seconds(20.0));

    UdpEchoClientHelper client4(Ipv4Address("192.9.39.5"), port);
    client4.SetAttribute("MaxPackets", UintegerValue(4));
    client4.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    client4.SetAttribute("PacketSize", UintegerValue(1024));
    clientApps = client4.Install(nodes.Get(3));
    clientApps.Start(Seconds(16.0));
    clientApps.Stop(Seconds(20.0));

    Simulator::Run();
    Simulator::Destroy();
    return 0;
}