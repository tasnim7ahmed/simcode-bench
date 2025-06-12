#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // Enable logging for debugging if needed
    // LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);

    Config::SetDefault ("ns3::CsmaNetDevice::Mtu", UintegerValue (1500));
    CommandLine cmd;
    cmd.Parse (argc, argv);

    // Nodes: 0-A, 1-B, 2-C
    NodeContainer nodes;
    nodes.Create (3);

    // 1. Point-to-Point links setup
    NodeContainer ab = NodeContainer(nodes.Get(0), nodes.Get(1));
    NodeContainer bc = NodeContainer(nodes.Get(1), nodes.Get(2));

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer abDevices = p2p.Install(ab);
    NetDeviceContainer bcDevices = p2p.Install(bc);

    // 2. CsmaNetDevices for /32 loopback-like interface
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay", StringValue("1us"));

    NetDeviceContainer csmaA;
    NetDeviceContainer csmaC;

    csmaA = csma.Install(NodeContainer(nodes.Get(0)));
    csmaC = csma.Install(NodeContainer(nodes.Get(2)));

    // 3. Internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper ipv4ab, ipv4bc;
    ipv4ab.SetBase("10.0.0.0", "255.255.255.252");
    ipv4bc.SetBase("10.0.1.0", "255.255.255.252");

    Ipv4InterfaceContainer abIfs = ipv4ab.Assign(abDevices);
    Ipv4InterfaceContainer bcIfs = ipv4bc.Assign(bcDevices);

    // 4. Assign /32 addresses to CsmaNetDevices (like loopback)
    Ptr<Ipv4> ipv4A = nodes.Get(0)->GetObject<Ipv4>();
    Ptr<Ipv4> ipv4C = nodes.Get(2)->GetObject<Ipv4>();
    Ptr<NetDevice> csmaDevA = csmaA.Get(0);
    Ptr<NetDevice> csmaDevC = csmaC.Get(0);

    int32_t ifA = ipv4A->AddInterface(csmaDevA);
    int32_t ifC = ipv4C->AddInterface(csmaDevC);

    Ipv4InterfaceAddress addrA = Ipv4InterfaceAddress("172.16.1.1", "255.255.255.255");
    Ipv4InterfaceAddress addrC = Ipv4InterfaceAddress("192.168.1.1", "255.255.255.255");
    ipv4A->AddAddress(ifA, addrA);
    ipv4A->SetUp(ifA);
    ipv4C->AddAddress(ifC, addrC);
    ipv4C->SetUp(ifC);

    // Mark these interfaces as "localhost" style (up, but not on-link for routing)
    // They will be reachable globally by their /32s via routing.

    // 5. Enable global routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // 6. Traffic: UDP flow from A(/32) to C(/32)
    uint16_t udpPort = 5000;

    // PacketSink on C, listening on its /32 interface
    Address sinkLocalAddress(InetSocketAddress(Ipv4Address("192.168.1.1"), udpPort));
    PacketSinkHelper packetSinkHelper("ns3::UdpSocketFactory", sinkLocalAddress);
    ApplicationContainer sinkApp = packetSinkHelper.Install(nodes.Get(2));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(10.0));

    // UDP client on A: send to C's /32 (192.168.1.1)
    UdpClientHelper udpClient(Ipv4Address("192.168.1.1"), udpPort);
    udpClient.SetAttribute("MaxPackets", UintegerValue(10000));
    udpClient.SetAttribute("Interval", TimeValue(Seconds(0.01)));
    udpClient.SetAttribute("PacketSize", UintegerValue(512));
    ApplicationContainer clientApp = udpClient.Install(nodes.Get(0));
    clientApp.Start(Seconds(1.0));
    clientApp.Stop(Seconds(9.9));

    // 7. Tracing and Packet Capture
    p2p.EnablePcapAll("three-router-p2p", true);
    csma.EnablePcap("three-router-csma-A", csmaA.Get(0), true);
    csma.EnablePcap("three-router-csma-C", csmaC.Get(0), true);

    AsciiTraceHelper ascii;
    p2p.EnableAsciiAll(ascii.CreateFileStream("three-router.tr"));

    // 8. Run simulation
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}