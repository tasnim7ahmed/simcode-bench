#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("RingTopologyExample");

int main(int argc, char *argv[])
{
    LogComponentEnable("RingTopologyExample", LOG_LEVEL_INFO);

    // Create 4 nodes
    NodeContainer nodes;
    nodes.Create(4);

    // Create 4 point-to-point links for the ring
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("1ms"));

    NetDeviceContainer devices[4];
    for (uint32_t i = 0; i < 4; ++i)
    {
        NodeContainer pair;
        pair.Add(nodes.Get(i));
        pair.Add(nodes.Get((i + 1) % 4));
        devices[i] = p2p.Install(pair);
    }

    // Install internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    Ipv4InterfaceContainer interfaces[4];
    for (uint32_t i = 0; i < 4; ++i)
    {
        std::ostringstream subnet;
        subnet << "10.1." << i + 1 << ".0";
        address.SetBase(subnet.str().c_str(), "255.255.255.0");
        interfaces[i] = address.Assign(devices[i]);
    }

    // Set up UDP Echo applications: each node sends to its clockwise neighbor
    uint16_t port = 9;
    ApplicationContainer serverApps, clientApps;
    for (uint32_t i = 0; i < 4; ++i)
    {
        // Install Echo Server on the neighbor
        UdpEchoServerHelper echoServer(port);
        ApplicationContainer server = echoServer.Install(nodes.Get((i + 1) % 4));
        server.Start(Seconds(1.0));
        server.Stop(Seconds(10.0));
        serverApps.Add(server);

        // Neighbor's ip address on the link connecting to current node
        Ipv4Address neighborAddr = interfaces[i].GetAddress(1);

        // Install Echo Client on the current node targeting the neighbor
        UdpEchoClientHelper echoClient(neighborAddr, port);
        echoClient.SetAttribute("MaxPackets", UintegerValue(3));
        echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
        echoClient.SetAttribute("PacketSize", UintegerValue(1024));
        ApplicationContainer client = echoClient.Install(nodes.Get(i));
        client.Start(Seconds(2.0));
        client.Stop(Seconds(10.0));
        clientApps.Add(client);
    }

    // Tracing packet transmissions
    for (uint32_t i = 0; i < 4; ++i)
    {
        devices[i].Get(0)->TraceConnectWithoutContext(
            "PhyTxEnd",
            MakeCallback([](Ptr<const Packet> packet) {
                NS_LOG_INFO("Packet transmitted at " << Simulator::Now().GetSeconds() << "s, size: " << packet->GetSize() << " bytes");
            })
        );
        devices[i].Get(1)->TraceConnectWithoutContext(
            "PhyTxEnd",
            MakeCallback([](Ptr<const Packet> packet) {
                NS_LOG_INFO("Packet transmitted at " << Simulator::Now().GetSeconds() << "s, size: " << packet->GetSize() << " bytes");
            })
        );
    }

    Simulator::Stop(Seconds(12.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}