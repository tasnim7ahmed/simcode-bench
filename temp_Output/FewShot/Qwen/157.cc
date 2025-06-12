#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/packet.h"
#include <fstream>

using namespace ns3;

std::ofstream g_csvFile;

void PacketRxTrace(Ptr<const Packet> packet, const Address& srcAddr, const Address& dstAddr, Time txTime, Time rxTime) {
    Ipv4Address srcIp = InetSocketAddress::ConvertFrom(srcAddr).GetIpv4();
    Ipv4Address dstIp = InetSocketAddress::ConvertFrom(dstAddr).GetIpv4();
    uint32_t size = packet->GetSize();

    g_csvFile << srcIp << "," << dstIp << "," << size << "," << txTime.GetSeconds() << "," << rxTime.GetSeconds() << std::endl;
}

int main(int argc, char *argv[]) {
    // Open CSV file and write header
    g_csvFile.open("ring_topology_packets.csv");
    g_csvFile << "Source,Destination,Size,TransmitTime,ReceiveTime" << std::endl;

    // Create 4 nodes in a ring topology
    NodeContainer nodes;
    nodes.Create(4);

    // Point-to-point link configuration
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("5ms"));

    // Connect each node to its neighbor (0-1, 1-2, 2-3, 3-0)
    NetDeviceContainer devices[4];
    for (int i = 0; i < 4; ++i) {
        NodeContainer pair = NodeContainer(nodes.Get(i), nodes.Get((i + 1) % 4));
        devices[i] = p2p.Install(pair);
    }

    // Install internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    Ipv4InterfaceContainer interfaces[4];
    for (int i = 0; i < 4; ++i) {
        std::ostringstream subnet;
        subnet << "10.0." << i << ".0";
        address.SetBase(subnet.str().c_str(), "255.255.255.0");
        interfaces[i] = address.Assign(devices[i]);
    }

    // Set up UDP echo servers on all nodes
    uint16_t port = 9;
    UdpEchoServerHelper server(port);
    ApplicationContainer serverApps;
    for (int i = 0; i < 4; ++i) {
        serverApps.Add(server.Install(nodes.Get(i)));
    }
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // Set up UDP echo clients sending every 1 second
    ApplicationContainer clientApps;
    for (int i = 0; i < 4; ++i) {
        Ipv4Address destIp = interfaces[(i + 1) % 4].GetAddress(0); // Destination is next node's IP
        UdpEchoClientHelper client(destIp, port);
        client.SetAttribute("MaxPackets", UintegerValue(8));
        client.SetAttribute("Interval", TimeValue(Seconds(1.0)));
        client.SetAttribute("PacketSize", UintegerValue(512));
        clientApps.Add(client.Install(nodes.Get(i)));
    }
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Hook the trace source for capturing packet TX/RX times
    Config::Connect("/NodeList/*/ApplicationList/*/$ns3::UdpEchoClient/PacketSent",
                    MakeBoundCallback(&PacketRxTrace), nullptr);

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    // Close CSV file
    g_csvFile.close();

    return 0;
}