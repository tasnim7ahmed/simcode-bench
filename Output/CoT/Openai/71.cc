#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // Set up logging if desired
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

    // Create nodes
    NodeContainer nodes;
    nodes.Create(3); // 0:A 1:B 2:C

    Ptr<Node> nodeA = nodes.Get(0);
    Ptr<Node> nodeB = nodes.Get(1);
    Ptr<Node> nodeC = nodes.Get(2);

    // Point-to-point link helpers
    PointToPointHelper p2p_ab;
    p2p_ab.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p_ab.SetChannelAttribute("Delay", StringValue("2ms"));

    PointToPointHelper p2p_bc;
    p2p_bc.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p_bc.SetChannelAttribute("Delay", StringValue("2ms"));

    // Create point-to-point links
    NodeContainer ab(nodeA, nodeB);
    NodeContainer bc(nodeB, nodeC);

    NetDeviceContainer dev_ab = p2p_ab.Install(ab);
    NetDeviceContainer dev_bc = p2p_bc.Install(bc);

    // Create CSMA devices for nodeA and nodeC (standalone /32)
    CsmaHelper csmaA;
    csmaA.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csmaA.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));

    NetDeviceContainer csmaDevA;
    csmaDevA.Add(csmaA.Install(nodeA).Get(0));

    CsmaHelper csmaC;
    csmaC.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csmaC.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));

    NetDeviceContainer csmaDevC;
    csmaDevC.Add(csmaC.Install(nodeC).Get(0));

    // Install Internet stack
    InternetStackHelper internet;
    internet.Install(nodes);

    // Assign IP addresses
    // a.a.a.a/32, x.x.x.0/30, y.y.y.0/30, c.c.c.c/32, 172.16.1.1/32, 192.168.1.1/32
    // Let's use: a.a.a.a = 10.0.0.1/32, x.x.x.0/30 = 10.0.1.0/30, y.y.y.0/30 = 10.0.2.0/30, c.c.c.c = 10.0.3.1/32

    // Point-to-point subnet A-B
    Ipv4AddressHelper ipv4ab;
    ipv4ab.SetBase("10.0.1.0", "255.255.255.252");
    Ipv4InterfaceContainer iface_ab = ipv4ab.Assign(dev_ab);

    // Point-to-point subnet B-C
    Ipv4AddressHelper ipv4bc;
    ipv4bc.SetBase("10.0.2.0", "255.255.255.252");
    Ipv4InterfaceContainer iface_bc = ipv4bc.Assign(dev_bc);

    // Assign /32 to nodeA's CSMA
    Ipv4AddressHelper ipv4csmaA;
    ipv4csmaA.SetBase("172.16.1.1", "255.255.255.255");
    Ipv4InterfaceContainer iface_csmaA = ipv4csmaA.Assign(csmaDevA);

    // Assign /32 to nodeC's CSMA
    Ipv4AddressHelper ipv4csmaC;
    ipv4csmaC.SetBase("192.168.1.1", "255.255.255.255");
    Ipv4InterfaceContainer iface_csmaC = ipv4csmaC.Assign(csmaDevC);

    // Assign a.a.a.a/32 to nodeA's loopback
    Ptr<Ipv4> ipv4A = nodeA->GetObject<Ipv4>();
    int32_t loA = ipv4A->AddInterface(0);
    ipv4A->AddAddress(1, Ipv4InterfaceAddress("10.0.0.1", "255.255.255.255"));  // (loopback/hostonly address)
    // But NS-3 reserves ifIndex 0 for loopback. We'll instead treat "10.0.0.1/32" as a dummy address on nodeA.

    // Assign c.c.c.c/32 to nodeC's loopback
    Ptr<Ipv4> ipv4C = nodeC->GetObject<Ipv4>();
    int32_t loC = ipv4C->AddInterface(0);
    ipv4C->AddAddress(1, Ipv4InterfaceAddress("10.0.3.1", "255.255.255.255"));

    // Enable static global routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // UDP packet sink on nodeC (on CSMA interface 192.168.1.1)
    uint16_t udpPort = 4000;
    Address sinkAddress(InetSocketAddress(Ipv4Address("192.168.1.1"), udpPort));
    UdpServerHelper udpServer(udpPort);
    ApplicationContainer serverApps = udpServer.Install(nodeC);
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // UDP client on nodeA, sending to sinkAddress (C's csma interface)
    uint32_t maxPackets = 1e6;
    Time interPacketInterval = Seconds(0.05); // 20 packets/sec, constant rate
    uint32_t packetSize = 1024;

    UdpClientHelper udpClient(sinkAddress);
    udpClient.SetAttribute("MaxPackets", UintegerValue(maxPackets));
    udpClient.SetAttribute("Interval", TimeValue(interPacketInterval));
    udpClient.SetAttribute("PacketSize", UintegerValue(packetSize));

    ApplicationContainer clientApps = udpClient.Install(nodeA);
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Enable pcap tracing
    p2p_ab.EnablePcapAll("ab-link", true);
    p2p_bc.EnablePcapAll("bc-link", true);
    csmaA.EnablePcap("csma-a", csmaDevA.Get(0), true, true);
    csmaC.EnablePcap("csma-c", csmaDevC.Get(0), true, true);

    // Enable ASCII tracing
    AsciiTraceHelper ascii;
    p2p_ab.EnableAsciiAll(ascii.CreateFileStream("ab-link.tr"));
    p2p_bc.EnableAsciiAll(ascii.CreateFileStream("bc-link.tr"));
    csmaA.EnableAscii(ascii.CreateFileStream("csma-a.tr"), csmaDevA.Get(0));
    csmaC.EnableAscii(ascii.CreateFileStream("csma-c.tr"), csmaDevC.Get(0));

    Simulator::Stop(Seconds(11.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}