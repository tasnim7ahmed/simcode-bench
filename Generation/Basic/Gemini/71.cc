#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/csma-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ThreeRouterNetwork");

int main(int argc, char* argv[])
{
    // Enable logging for UdpClient, PacketSink, and Global Routing
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("PacketSink", LOG_LEVEL_INFO);
    LogComponentEnable("Ipv4GlobalRouting", LOG_LEVEL_INFO);

    // Create three nodes: A, B, C
    NodeContainer nodes;
    nodes.Create(3);
    Ptr<Node> nA = nodes.Get(0); // Node A
    Ptr<Node> nB = nodes.Get(1); // Node B (router)
    Ptr<Node> nC = nodes.Get(2); // Node C

    // Install the Internet stack on all nodes
    InternetStackHelper internet;
    internet.Install(nodes);

    // Configure Point-to-Point links
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // Create A-B link
    NetDeviceContainer devicesA_B = p2p.Install(nA, nB);
    Ipv4AddressHelper ipv4A_B;
    ipv4A_B.SetBase("10.1.1.0", "255.255.255.252"); // Subnet x.x.x.0/30
    Ipv4InterfaceContainer interfacesA_B = ipv4A_B.Assign(devicesA_B);

    // Create B-C link
    NetDeviceContainer devicesB_C = p2p.Install(nB, nC);
    Ipv4AddressHelper ipv4B_C;
    ipv4B_C.SetBase("10.1.2.0", "255.255.255.252"); // Subnet y.y.y.0/30
    Ipv4InterfaceContainer interfacesB_C = ipv4B_C.Assign(devicesB_C);

    // Add CsmaNetDevices to nodes A and C for their /32 IP addresses
    // These interfaces represent the "host" interfaces for traffic source/sink.
    CsmaHelper csma;
    csma.SetChannelAttribute("Delay", StringValue("0ms")); // Minimal delay for host link

    // CSMA device on Node A for IP 172.16.1.1/32
    NetDeviceContainer csmaDevA = csma.Install(nA);
    Ipv4AddressHelper ipv4HostA;
    ipv4HostA.SetBase("172.16.1.1", "255.255.255.255"); // a /32 IP
    Ipv4InterfaceContainer csmaIfA = ipv4HostA.Assign(csmaDevA);

    // CSMA device on Node C for IP 192.168.1.1/32
    NetDeviceContainer csmaDevC = csma.Install(nC);
    Ipv4AddressHelper ipv4HostC;
    ipv4HostC.SetBase("192.168.1.1", "255.255.255.255"); // a /32 IP
    Ipv4InterfaceContainer csmaIfC = ipv4HostC.Assign(csmaDevC);

    // Enable Global Routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Set up UDP traffic from A to C
    uint16_t port = 9; // Discard port

    // Packet Sink (on Node C)
    // Listen on the assigned /32 IP address on Node C's CSMA interface
    Address sinkAddress(InetSocketAddress(csmaIfC.GetAddress(0), port));
    PacketSinkHelper packetSinkHelper("ns3::UdpSocketFactory", sinkAddress);
    ApplicationContainer sinkApps = packetSinkHelper.Install(nC);
    sinkApps.Start(Seconds(0.5)); // Start sink slightly before client
    sinkApps.Stop(Seconds(10.5)); // Stop sink slightly after client

    // UDP Client (on Node A)
    // Send to the assigned /32 IP address on Node C
    UdpClientHelper udpClientHelper(csmaIfC.GetAddress(0), port);
    udpClientHelper.SetAttribute("MaxPackets", UintegerValue(100));     // Send 100 packets
    udpClientHelper.SetAttribute("Interval", TimeValue(Seconds(0.01))); // Send every 0.01 seconds (100 packets/sec)
    udpClientHelper.SetAttribute("PacketSize", UintegerValue(1024));    // 1KB packets
    ApplicationContainer clientApps = udpClientHelper.Install(nA);
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(10.0));

    // Enable Tracing and Packet Capturing
    // PCAP traces for all devices
    p2p.EnablePcap("three-router-ab", devicesA_B.Get(0), true); // Node A side of A-B
    p2p.EnablePcap("three-router-ab", devicesA_B.Get(1), true); // Node B side of A-B
    p2p.EnablePcap("three-router-bc", devicesB_C.Get(0), true); // Node B side of B-C
    p2p.EnablePcap("three-router-bc", devicesB_C.Get(1), true); // Node C side of B-C
    csma.EnablePcap("three-router-host-a", csmaDevA.Get(0), true); // CSMA device on Node A
    csma.EnablePcap("three-router-host-c", csmaDevC.Get(0), true); // CSMA device on Node C

    // ASCII traces for selected devices
    AsciiTraceHelper asciiTraceHelper;
    p2p.EnableAscii("three-router-ab.tr", devicesA_B.Get(0));
    p2p.EnableAscii("three-router-bc.tr", devicesB_C.Get(1));
    csma.EnableAscii("three-router-host-a.tr", csmaDevA.Get(0));
    csma.EnableAscii("three-router-host-c.tr", csmaDevC.Get(0));

    // Set simulation stop time
    Simulator::Stop(Seconds(11.0));

    // Run the simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}