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

    // LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    // LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create nodes: 3 routers, 4 subnetworks (each subnetwork has a node+router)
    NodeContainer routers;
    routers.Create(3);

    NodeContainer lan1;
    lan1.Create(1);
    NodeContainer lan2;
    lan2.Create(1);
    NodeContainer lan3;
    lan3.Create(1);
    NodeContainer lan4;
    lan4.Create(1);

    // Create point-to-point links between routers (assume R0, R1, R2)
    NodeContainer r0r1 = NodeContainer(routers.Get(0), routers.Get(1));
    NodeContainer r1r2 = NodeContainer(routers.Get(1), routers.Get(2));
    NodeContainer r0r2 = NodeContainer(routers.Get(0), routers.Get(2));

    // Connect each subnetwork to one router (one hop)
    NodeContainer lan1r = NodeContainer(lan1.Get(0), routers.Get(0));
    NodeContainer lan2r = NodeContainer(lan2.Get(0), routers.Get(0));
    NodeContainer lan3r = NodeContainer(lan3.Get(0), routers.Get(1));
    NodeContainer lan4r = NodeContainer(lan4.Get(0), routers.Get(2));

    // Set up point-to-point helpers
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // Install devices: router-router links
    NetDeviceContainer d_r0r1 = p2p.Install(r0r1);
    NetDeviceContainer d_r1r2 = p2p.Install(r1r2);
    NetDeviceContainer d_r0r2 = p2p.Install(r0r2);

    // router-LAN links
    NetDeviceContainer d_lan1r = p2p.Install(lan1r);
    NetDeviceContainer d_lan2r = p2p.Install(lan2r);
    NetDeviceContainer d_lan3r = p2p.Install(lan3r);
    NetDeviceContainer d_lan4r = p2p.Install(lan4r);

    // Install the Internet stack using RIPng (Distance Vector Routing)
    // However, in IPv4, RIP is not available, OSPF and RIPng are for IPv6.
    // Instead, use Ipv4GlobalRouting (simplest case) to enable dynamic routing
    InternetStackHelper stack;
    stack.SetRoutingHelper(Ipv4GlobalRoutingHelper());
    stack.Install(routers);
    stack.Install(lan1);
    stack.Install(lan2);
    stack.Install(lan3);
    stack.Install(lan4);

    // Assign IP addresses
    Ipv4AddressHelper ipv4;

    // Subnets
    // Router 0 - LAN1        : 10.1.1.0/24
    // Router 0 - LAN2        : 10.1.2.0/24
    // Router 1 - LAN3        : 10.1.3.0/24
    // Router 2 - LAN4        : 10.1.4.0/24
    // Router 0 <-> Router 1  : 10.1.5.0/24
    // Router 1 <-> Router 2  : 10.1.6.0/24
    // Router 0 <-> Router 2  : 10.1.7.0/24

    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer i_lan1r = ipv4.Assign(d_lan1r);

    ipv4.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer i_lan2r = ipv4.Assign(d_lan2r);

    ipv4.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer i_lan3r = ipv4.Assign(d_lan3r);

    ipv4.SetBase("10.1.4.0", "255.255.255.0");
    Ipv4InterfaceContainer i_lan4r = ipv4.Assign(d_lan4r);

    ipv4.SetBase("10.1.5.0", "255.255.255.0");
    Ipv4InterfaceContainer i_r0r1 = ipv4.Assign(d_r0r1);

    ipv4.SetBase("10.1.6.0", "255.255.255.0");
    Ipv4InterfaceContainer i_r1r2 = ipv4.Assign(d_r1r2);

    ipv4.SetBase("10.1.7.0", "255.255.255.0");
    Ipv4InterfaceContainer i_r0r2 = ipv4.Assign(d_r0r2);

    // Enable global routing tables calculation (Distance Vector action)
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Set up echo server on LAN4 node
    uint16_t port = 9;
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApps = echoServer.Install(lan4.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // Client on LAN1 node, send to LAN4 node
    Ipv4Address destAddr = i_lan4r.GetAddress(0);
    UdpEchoClientHelper echoClient(destAddr, port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(64));
    ApplicationContainer clientApps = echoClient.Install(lan1.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Enable pcap tracing on all point-to-point devices
    p2p.EnablePcapAll("dvr");

    // Also enable ASCII routing table output at time 2s
    Ptr<OutputStreamWrapper> routingStream = Create<OutputStreamWrapper>("routing-tables.log", std::ios::out);
    Ipv4GlobalRoutingHelper routingHelper;
    Simulator::Schedule(Seconds(2.0), &Ipv4GlobalRoutingHelper::PrintRoutingTableAllAt, routingHelper, Seconds(2.0), routingStream);

    Simulator::Stop(Seconds(11.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}