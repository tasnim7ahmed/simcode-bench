#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/csma-module.h"
#include "ns3/mobility-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("RingTopologySimulation");

int main(int argc, char *argv[]) {
    // Enable logging for packet transmissions
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create 4 nodes
    NodeContainer nodes;
    nodes.Create(4);

    // Create point-to-point links between each pair of adjacent nodes to form a ring
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("1ms"));

    NetDeviceContainer devices[4];

    // Connect nodes in a ring: 0-1, 1-2, 2-3, 3-0
    devices[0] = p2p.Install(nodes.Get(0), nodes.Get(1));
    devices[1] = p2p.Install(nodes.Get(1), nodes.Get(2));
    devices[2] = p2p.Install(nodes.Get(2), nodes.Get(3));
    devices[3] = p2p.Install(nodes.Get(3), nodes.Get(0));

    // Install Internet stack on all nodes
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    Ipv4InterfaceContainer interfaces[4];

    address.SetBase("10.1.1.0", "255.255.255.0");
    interfaces[0] = address.Assign(devices[0]);
    address.NewNetwork();
    interfaces[1] = address.Assign(devices[1]);
    address.NewNetwork();
    interfaces[2] = address.Assign(devices[2]);
    address.NewNetwork();
    interfaces[3] = address.Assign(devices[3]);

    // Set up echo server applications on all nodes (port 9)
    UdpEchoServerHelper echoServer(9);

    ApplicationContainer serverApps[4];
    for (int i = 0; i < 4; ++i) {
        serverApps[i] = echoServer.Install(nodes.Get(i));
        serverApps[i].Start(Seconds(1.0));
        serverApps[i].Stop(Seconds(10.0));
    }

    // Set up echo client applications on each node sending to the next node in the ring
    UdpEchoClientHelper echoClient(interfaces[0].GetAddress(1), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps[4];
    for (int i = 0; i < 4; ++i) {
        Address serverAddress = (i == 3) ? interfaces[3].GetAddress(1) : interfaces[i].GetAddress(1);
        uint32_t nextNodeIndex = (i + 1) % 4;
        Ipv4Address nextNodeIp = (nextNodeIndex == 0) ? interfaces[3].GetAddress(1) :
                                 (nextNodeIndex == 1) ? interfaces[0].GetAddress(0) :
                                 (nextNodeIndex == 2) ? interfaces[1].GetAddress(0) :
                                                        interfaces[2].GetAddress(0);

        UdpEchoClientHelper client(nextNodeIp, 9);
        client.SetAttribute("MaxPackets", UintegerValue(5));
        client.SetAttribute("Interval", TimeValue(Seconds(2.0)));
        client.SetAttribute("PacketSize", UintegerValue(512));

        clientApps[i] = client.Install(nodes.Get(i));
        clientApps[i].Start(Seconds(2.0));
        clientApps[i].Stop(Seconds(10.0));
    }

    // Enable global routing for connectivity across multiple hops
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Run the simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}