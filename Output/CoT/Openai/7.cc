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

    // Create point-to-point links for the ring
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // Containers for device pairs and interface pairs
    NetDeviceContainer devices[4];
    Ipv4InterfaceContainer interfaces[4];

    // Connect nodes in a ring: 0-1, 1-2, 2-3, 3-0
    for (uint32_t i = 0; i < 4; ++i)
    {
        devices[i] = p2p.Install(nodes.Get(i), nodes.Get((i + 1) % 4));
    }

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses in 192.9.39.0/24
    Ipv4AddressHelper address;
    for (uint32_t i = 0; i < 4; ++i)
    {
        std::ostringstream subnet;
        subnet << "192.9.39." << (i * 10) << "/30"; // small subnets for each link
        address.SetBase(Ipv4Address(subnet.str().c_str()), "255.255.255.252");
        interfaces[i] = address.Assign(devices[i]);
        address.NewNetwork();
    }

    // Set up UDP Echo Server and Client applications in a rotation, 
    // so only one pair is active at any time. Each node acts as both client and server.
    std::vector<ApplicationContainer> serverApps(4);
    std::vector<ApplicationContainer> clientApps(4);

    double startTime = 1.0;
    double stopTime = 7.0;

    // The rotation: [client, server] pairs: [0,1], [1,2], [2,3], [3,0]
    for (uint32_t i = 0; i < 4; ++i)
    {
        uint32_t serverNode = (i + 1) % 4;
        uint32_t clientNode = i;

        // UDP Echo Server
        uint16_t port = 9;
        UdpEchoServerHelper echoServer(port);
        serverApps[i] = echoServer.Install(nodes.Get(serverNode));
        serverApps[i].Start(Seconds(startTime + i * 1.5));
        serverApps[i].Stop(Seconds(startTime + (i + 1) * 1.5));

        // UDP Echo Client
        // Get server address on the interface facing the client
        // Each node has two net devices/interfaces:
        // node 0: devices[0] (to 1), devices[3] (to 3)
        // node 1: devices[0] (to 0), devices[1] (to 2)
        // node 2: devices[1] (to 1), devices[2] (to 3)
        // node 3: devices[2] (to 2), devices[3] (to 0)

        Address serverAddress;
        // On each link, interfaces[i].GetAddress(0) <-> node i
        //                         interfaces[i].GetAddress(1) <-> node (i+1)%4
        // So from clientNode to serverNode, interface is devices[i], address index is 1
        serverAddress = interfaces[i].GetAddress(1);

        UdpEchoClientHelper echoClient(serverAddress, port);
        echoClient.SetAttribute("MaxPackets", UintegerValue(4));
        echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
        echoClient.SetAttribute("PacketSize", UintegerValue(64));
        clientApps[i] = echoClient.Install(nodes.Get(clientNode));
        clientApps[i].Start(Seconds(startTime + i * 1.5 + 0.1));
        clientApps[i].Stop(Seconds(startTime + (i + 1) * 1.5));
    }

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    Simulator::Stop(Seconds(stopTime));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}