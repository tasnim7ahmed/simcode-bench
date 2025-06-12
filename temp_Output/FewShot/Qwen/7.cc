#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable logging for debugging
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create four nodes in a ring topology
    NodeContainer nodes;
    nodes.Create(4);

    // Point-to-point helper for links
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    // Containers to hold net devices and interfaces
    NetDeviceContainer devices[4];
    Ipv4InterfaceContainer interfaces[4];

    // Connect nodes in a ring: 0-1, 1-2, 2-3, 3-0
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses from the 192.9.39.0/24 network
    Ipv4AddressHelper address;
    address.SetBase("192.9.39.0", "255.255.255.0");

    // Create links between adjacent nodes (0-1, 1-2, 2-3, 3-0)
    for (uint32_t i = 0; i < 4; ++i) {
        NodeContainer pair = NodeContainer(nodes.Get(i), nodes.Get((i + 1) % 4));
        devices[i] = pointToPoint.Install(pair);
        interfaces[i] = address.Assign(devices[i]);
        address.NewNetwork();
    }

    // Set up one client-server application at a time
    uint16_t port = 9;

    // Time control variables
    double startTime = 1.0;
    double interval = 2.0;

    for (uint32_t i = 0; i < 4; ++i) {
        uint32_t serverNodeIndex = (i + 1) % 4;
        uint32_t clientNodeIndex = i;

        // Server on node serverNodeIndex
        UdpEchoServerHelper echoServer(port);
        ApplicationContainer serverApp = echoServer.Install(nodes.Get(serverNodeIndex));
        serverApp.Start(Seconds(startTime));
        serverApp.Stop(Seconds(startTime + interval));

        // Client on node clientNodeIndex
        Ipv4Address serverIp = interfaces[serverNodeIndex]->GetAddress(1);
        UdpEchoClientHelper echoClient(serverIp, port);
        echoClient.SetAttribute("MaxPackets", UintegerValue(4));
        echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
        echoClient.SetAttribute("PacketSize", UintegerValue(1024));

        ApplicationContainer clientApp = echoClient.Install(nodes.Get(clientNodeIndex));
        clientApp.Start(Seconds(startTime + 0.5));
        clientApp.Stop(Seconds(startTime + interval));

        startTime += interval;
    }

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}