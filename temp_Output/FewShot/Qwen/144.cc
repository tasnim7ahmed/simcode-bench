#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable logging
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create 4 nodes for the ring topology
    NodeContainer nodes;
    nodes.Create(4);

    // Configure point-to-point links
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("1ms"));

    // Connect nodes in a ring
    NetDeviceContainer devices[4];
    devices[0] = p2p.Install(nodes.Get(0), nodes.Get(1));
    devices[1] = p2p.Install(nodes.Get(1), nodes.Get(2));
    devices[2] = p2p.Install(nodes.Get(2), nodes.Get(3));
    devices[3] = p2p.Install(nodes.Get(3), nodes.Get(0));

    // Install internet stack on all nodes
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses to each link
    Ipv4AddressHelper address;
    Ipv4InterfaceContainer interfaces[4];

    address.SetBase("10.0.0.0", "255.255.255.0");
    interfaces[0] = address.Assign(devices[0]);
    address.NewNetwork();
    interfaces[1] = address.Assign(devices[1]);
    address.NewNetwork();
    interfaces[2] = address.Assign(devices[2]);
    address.NewNetwork();
    interfaces[3] = address.Assign(devices[3]);

    // Set up UDP Echo Server on each node
    uint16_t port = 9;
    ApplicationContainer serverApps;
    for (int i = 0; i < 4; ++i) {
        UdpEchoServerHelper echoServer(port);
        serverApps = echoServer.Install(nodes.Get(i));
        serverApps.Start(Seconds(1.0));
        serverApps.Stop(Seconds(10.0));
    }

    // Set up UDP Echo Client on each node sending to the next node in the ring
    ApplicationContainer clientApps;
    for (int i = 0; i < 4; ++i) {
        int nextNode = (i + 1) % 4;
        Ipv4Address nextNodeIp;

        if (nextNode == (i + 1) % 4) {
            nextNodeIp = interfaces[(i + 1) % 4].GetAddress(0);
        } else {
            nextNodeIp = interfaces[nextNode].GetAddress(1);
        }

        UdpEchoClientHelper echoClient(nextNodeIp, port);
        echoClient.SetAttribute("MaxPackets", UintegerValue(5));
        echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
        echoClient.SetAttribute("PacketSize", UintegerValue(512));

        clientApps = echoClient.Install(nodes.Get(i));
        clientApps.Start(Seconds(2.0));
        clientApps.Stop(Seconds(10.0));
    }

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}