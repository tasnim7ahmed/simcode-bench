#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("RingTopologySimulation");

int main(int argc, char *argv[])
{
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

    // Create 4 nodes
    NodeContainer nodes;
    nodes.Create(4);

    // Define links between adjacent pairs to form a ring
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("1ms"));

    // Each pair: (n0-n1), (n1-n2), (n2-n3), (n3-n0)
    NetDeviceContainer devices[4];
    devices[0] = p2p.Install(nodes.Get(0), nodes.Get(1));
    devices[1] = p2p.Install(nodes.Get(1), nodes.Get(2));
    devices[2] = p2p.Install(nodes.Get(2), nodes.Get(3));
    devices[3] = p2p.Install(nodes.Get(3), nodes.Get(0));

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses per link
    Ipv4AddressHelper address;
    Ipv4InterfaceContainer interfaces[4];
    address.SetBase("10.1.1.0", "255.255.255.0");
    interfaces[0] = address.Assign(devices[0]);
    address.SetBase("10.1.2.0", "255.255.255.0");
    interfaces[1] = address.Assign(devices[1]);
    address.SetBase("10.1.3.0", "255.255.255.0");
    interfaces[2] = address.Assign(devices[2]);
    address.SetBase("10.1.4.0", "255.255.255.0");
    interfaces[3] = address.Assign(devices[3]);

    // UDP servers on each node (listen on port 4000)
    uint16_t udpPort = 4000;
    ApplicationContainer serverApps;
    for (uint32_t i = 0; i < 4; ++i) {
        UdpServerHelper server(udpPort);
        serverApps.Add(server.Install(nodes.Get(i)));
    }
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // UDP Clients on each node. Each node sends to its next neighbor in the ring
    ApplicationContainer clientApps;
    for (uint32_t i = 0; i < 4; ++i) {
        uint32_t neighbor = (i + 1) % 4;

        // Find the IP address of the neighbor on the link connecting them
        // The 2nd address of each link goes to the second node argument in Install()
        // (n0-n1): n0->10.1.1.1, n1->10.1.1.2
        // (n1-n2): n1->10.1.2.1, n2->10.1.2.2
        // (n2-n3): n2->10.1.3.1, n3->10.1.3.2
        // (n3-n0): n3->10.1.4.1, n0->10.1.4.2

        // Each node sends to its next neighbor using the IP address on the link between them
        Ipv4Address dstAddr;
        if (i == 0) {
            // n0 sends to n1 via 10.1.1.2
            dstAddr = interfaces[0].GetAddress(1);
        } else if (i == 1) {
            // n1 sends to n2 via 10.1.2.2
            dstAddr = interfaces[1].GetAddress(1);
        } else if (i == 2) {
            // n2 sends to n3 via 10.1.3.2
            dstAddr = interfaces[2].GetAddress(1);
        } else if (i == 3) {
            // n3 sends to n0 via 10.1.4.2
            dstAddr = interfaces[3].GetAddress(1);
        }

        UdpClientHelper client(dstAddr, udpPort);
        client.SetAttribute("MaxPackets", UintegerValue(10));
        client.SetAttribute("Interval", TimeValue(Seconds(1.0)));
        client.SetAttribute("PacketSize", UintegerValue(1024));
        clientApps.Add(client.Install(nodes.Get(i)));
    }
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(9.0));

    // Routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}