#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    CommandLine cmd;
    cmd.Parse(argc, argv);

    // Create 4 nodes
    NodeContainer nodes;
    nodes.Create(4);

    // Create point-to-point links to form a ring: 0-1-2-3-0
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer d01 = p2p.Install(nodes.Get(0), nodes.Get(1));
    NetDeviceContainer d12 = p2p.Install(nodes.Get(1), nodes.Get(2));
    NetDeviceContainer d23 = p2p.Install(nodes.Get(2), nodes.Get(3));
    NetDeviceContainer d30 = p2p.Install(nodes.Get(3), nodes.Get(0));

    // Assign IP addresses
    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("192.9.39.0", "255.255.255.0");
    Ipv4InterfaceContainer if01 = address.Assign(d01);
    address.NewNetwork();
    Ipv4InterfaceContainer if12 = address.Assign(d12);
    address.NewNetwork();
    Ipv4InterfaceContainer if23 = address.Assign(d23);
    address.NewNetwork();
    Ipv4InterfaceContainer if30 = address.Assign(d30);

    // Populate routing tables
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // UDP Echo server and client setup
    uint16_t port = 9;
    double appInterval = 1.0;
    double appDuration = 2.5;
    Time pktInterval = Seconds(0.5);
    uint32_t maxPackets = 4;

    ApplicationContainer serverApps;
    ApplicationContainer clientApps;

    // Communication pairs (client node, server node, server address)
    struct CommPair
    {
        uint32_t client;
        uint32_t server;
        Ipv4Address serverAddr;
        double startTime;
    };

    std::vector<CommPair> pairs = {
        {0, 1, if01.GetAddress(1), 1.0},
        {1, 2, if12.GetAddress(1), 4.0},
        {2, 3, if23.GetAddress(1), 7.0},
        {3, 0, if30.GetAddress(1), 10.0}
    };

    for (auto &p : pairs)
    {
        UdpEchoServerHelper echoServer(port);
        ApplicationContainer sa = echoServer.Install(nodes.Get(p.server));
        sa.Start(Seconds(p.startTime));
        sa.Stop(Seconds(p.startTime + appDuration));
        serverApps.Add(sa);

        UdpEchoClientHelper echoClient(p.serverAddr, port);
        echoClient.SetAttribute("MaxPackets", UintegerValue(maxPackets));
        echoClient.SetAttribute("Interval", TimeValue(pktInterval));
        echoClient.SetAttribute("PacketSize", UintegerValue(1024));
        ApplicationContainer ca = echoClient.Install(nodes.Get(p.client));
        ca.Start(Seconds(p.startTime + 0.5));
        ca.Stop(Seconds(p.startTime + appDuration));
        clientApps.Add(ca);
    }

    // NetAnim setup
    AnimationInterface anim("ring-topology.xml");
    anim.SetConstantPosition(nodes.Get(0), 50.0, 50.0);
    anim.SetConstantPosition(nodes.Get(1), 100.0, 100.0);
    anim.SetConstantPosition(nodes.Get(2), 50.0, 150.0);
    anim.SetConstantPosition(nodes.Get(3), 0.0, 100.0);

    Simulator::Stop(Seconds(13.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}