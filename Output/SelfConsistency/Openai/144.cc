#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("RingTopologySimulation");

int 
main(int argc, char *argv[])
{
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create 4 nodes
    NodeContainer nodes;
    nodes.Create(4);

    // Create point-to-point links
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("1ms"));

    // For a ring: node 0-1, 1-2, 2-3, 3-0
    NetDeviceContainer devices[4];
    for (uint32_t i = 0; i < 4; ++i)
    {
        devices[i] = p2p.Install(nodes.Get(i), nodes.Get((i+1)%4));
    }

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    std::array<Ipv4InterfaceContainer, 4> interfaces;
    for (uint32_t i = 0; i < 4; ++i)
    {
        std::ostringstream subnet;
        subnet << "10.1." << i+1 << ".0";
        address.SetBase(subnet.str().c_str(), "255.255.255.0");
        interfaces[i] = address.Assign(devices[i]);
    }

    // Set up UDP Echo Servers (on the "neighbor" side of each link)
    uint16_t port = 9;
    ApplicationContainer serverApps;
    for (uint32_t i = 0; i < 4; ++i)
    {
        // Install on node (i+1)%4
        UdpEchoServerHelper echoServer(port);
        serverApps.Add(echoServer.Install(nodes.Get((i+1)%4)));
    }
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // Set up UDP Echo Clients on each node, targeting the neighbor's interface
    ApplicationContainer clientApps;
    for (uint32_t i = 0; i < 4; ++i)
    {
        // Each node sends to its next neighbor
        uint32_t neighbor = (i+1)%4;
        // Each node's interface facing its neighbor is interfaces[i].GetAddress(1)
        UdpEchoClientHelper echoClient(interfaces[i].GetAddress(1), port);
        echoClient.SetAttribute("MaxPackets", UintegerValue(5));
        echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
        echoClient.SetAttribute("PacketSize", UintegerValue(1024));
        clientApps.Add(echoClient.Install(nodes.Get(i)));
    }
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Enable routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}