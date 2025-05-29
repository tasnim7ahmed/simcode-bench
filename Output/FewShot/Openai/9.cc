#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create four nodes for the ring
    NodeContainer nodes;
    nodes.Create(4);

    // Create point-to-point links between adjacent nodes (forming a ring)
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // Connect nodes: 0-1, 1-2, 2-3, 3-0
    NetDeviceContainer devices[4];
    devices[0] = p2p.Install(nodes.Get(0), nodes.Get(1));
    devices[1] = p2p.Install(nodes.Get(1), nodes.Get(2));
    devices[2] = p2p.Install(nodes.Get(2), nodes.Get(3));
    devices[3] = p2p.Install(nodes.Get(3), nodes.Get(0));

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    // Each subnet gets a /30 portion from main /24 (so 4 subnets)
    address.SetBase("192.9.39.0", "255.255.255.252"); // .0/30
    Ipv4InterfaceContainer interfaces0 = address.Assign(devices[0]); // 0-1
    address.NewNetwork(); // .4/30
    Ipv4InterfaceContainer interfaces1 = address.Assign(devices[1]); // 1-2
    address.NewNetwork(); // .8/30
    Ipv4InterfaceContainer interfaces2 = address.Assign(devices[2]); // 2-3
    address.NewNetwork(); // .12/30
    Ipv4InterfaceContainer interfaces3 = address.Assign(devices[3]); // 3-0

    // Application parameters
    uint16_t echoPort = 9;
    uint32_t maxPackets = 4;
    Time interval = Seconds(1.0);

    ApplicationContainer apps;

    // 1st pair: node 0 (client) -> node 2 (server)
    UdpEchoServerHelper echoServer1(echoPort);
    apps = echoServer1.Install(nodes.Get(2));
    apps.Start(Seconds(1.0));
    apps.Stop(Seconds(5.0));

    UdpEchoClientHelper echoClient1(Ipv4Address("192.9.39.10"), echoPort); // node 2's addr on 2-3
    echoClient1.SetAttribute("MaxPackets", UintegerValue(maxPackets));
    echoClient1.SetAttribute("Interval", TimeValue(interval));
    echoClient1.SetAttribute("PacketSize", UintegerValue(1024));
    apps = echoClient1.Install(nodes.Get(0));
    apps.Start(Seconds(1.5));
    apps.Stop(Seconds(5.0));

    // 2nd pair: node 1 (client) -> node 3 (server)
    UdpEchoServerHelper echoServer2(echoPort + 1);
    apps = echoServer2.Install(nodes.Get(3));
    apps.Start(Seconds(5.5));
    apps.Stop(Seconds(9.0));

    UdpEchoClientHelper echoClient2(Ipv4Address("192.9.39.13"), echoPort + 1); // node 3's addr on 3-0
    echoClient2.SetAttribute("MaxPackets", UintegerValue(maxPackets));
    echoClient2.SetAttribute("Interval", TimeValue(interval));
    echoClient2.SetAttribute("PacketSize", UintegerValue(1024));
    apps = echoClient2.Install(nodes.Get(1));
    apps.Start(Seconds(6.0));
    apps.Stop(Seconds(9.0));

    // 3rd pair: node 2 (client) -> node 0 (server)
    UdpEchoServerHelper echoServer3(echoPort + 2);
    apps = echoServer3.Install(nodes.Get(0));
    apps.Start(Seconds(9.5));
    apps.Stop(Seconds(13.0));

    UdpEchoClientHelper echoClient3(Ipv4Address("192.9.39.1"), echoPort + 2); // node 0's addr on 0-1
    echoClient3.SetAttribute("MaxPackets", UintegerValue(maxPackets));
    echoClient3.SetAttribute("Interval", TimeValue(interval));
    echoClient3.SetAttribute("PacketSize", UintegerValue(1024));
    apps = echoClient3.Install(nodes.Get(2));
    apps.Start(Seconds(10.0));
    apps.Stop(Seconds(13.0));

    // 4th pair: node 3 (client) -> node 1 (server)
    UdpEchoServerHelper echoServer4(echoPort + 3);
    apps = echoServer4.Install(nodes.Get(1));
    apps.Start(Seconds(13.5));
    apps.Stop(Seconds(17.0));

    UdpEchoClientHelper echoClient4(Ipv4Address("192.9.39.5"), echoPort + 3); // node 1's addr on 1-2
    echoClient4.SetAttribute("MaxPackets", UintegerValue(maxPackets));
    echoClient4.SetAttribute("Interval", TimeValue(interval));
    echoClient4.SetAttribute("PacketSize", UintegerValue(1024));
    apps = echoClient4.Install(nodes.Get(3));
    apps.Start(Seconds(14.0));
    apps.Stop(Seconds(17.0));

    // Set up NetAnim: assign node positions in a circle for visualization
    AnimationInterface anim("ring-topology.xml");
    double radius = 60.0;
    for (uint32_t i = 0; i < 4; ++i) {
        double theta = 2 * M_PI * i / 4;
        double x = 100.0 + radius * std::cos(theta);
        double y = 100.0 + radius * std::sin(theta);
        anim.SetConstantPosition(nodes.Get(i), x, y);
    }

    Simulator::Stop(Seconds(18.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}