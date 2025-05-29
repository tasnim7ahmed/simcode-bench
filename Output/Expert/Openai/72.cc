#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/bridge-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int
main(int argc, char *argv[])
{
    std::string lanDataRate = "100Mbps";
    std::string wanDataRate = "10Mbps";
    std::string lanDelay = "6560ns";
    std::string wanDelay = "10ms";

    CommandLine cmd;
    cmd.AddValue("lanDataRate", "LAN link data rate (e.g., 100Mbps or 10Mbps)", lanDataRate);
    cmd.AddValue("lanDelay", "LAN link delay", lanDelay);
    cmd.AddValue("wanDataRate", "WAN link data rate", wanDataRate);
    cmd.AddValue("wanDelay", "WAN link delay", wanDelay);
    cmd.Parse(argc, argv);

    // Names: t2 (top client), t3 (top server), tr (top router)
    //        b2 (bottom server), b3 (bottom client), br (bottom router)

    // Create all terminal and router nodes
    Ptr<Node> t2 = CreateObject<Node>();
    Ptr<Node> t3 = CreateObject<Node>();
    Ptr<Node> tr = CreateObject<Node>();

    Ptr<Node> b2 = CreateObject<Node>();
    Ptr<Node> b3 = CreateObject<Node>();
    Ptr<Node> br = CreateObject<Node>();

    // Create switch nodes (LAN switches)
    // Top: switches t_swA, t_swB (for t2 switch path), t_sw1 (for t3)
    Ptr<Node> t_swA = CreateObject<Node>();
    Ptr<Node> t_swB = CreateObject<Node>();
    Ptr<Node> t_sw1 = CreateObject<Node>();

    // Bottom: switches b_swA, b_swB (for b2 switch path), b_sw1 (for b3)
    Ptr<Node> b_swA = CreateObject<Node>();
    Ptr<Node> b_swB = CreateObject<Node>();
    Ptr<Node> b_sw1 = CreateObject<Node>();

    NodeContainer allNodes;
    allNodes.Add(t2);
    allNodes.Add(t3);
    allNodes.Add(tr);
    allNodes.Add(b2);
    allNodes.Add(b3);
    allNodes.Add(br);
    allNodes.Add(t_swA);
    allNodes.Add(t_swB);
    allNodes.Add(t_sw1);
    allNodes.Add(b_swA);
    allNodes.Add(b_swB);
    allNodes.Add(b_sw1);

    // Install Internet stack on end hosts and routers
    NodeContainer nodesToInstallStack;
    nodesToInstallStack.Add(t2);
    nodesToInstallStack.Add(t3);
    nodesToInstallStack.Add(tr);
    nodesToInstallStack.Add(b2);
    nodesToInstallStack.Add(b3);
    nodesToInstallStack.Add(br);

    InternetStackHelper stack;
    stack.Install(nodesToInstallStack);

    // TOP LAN: Build switch structure:
    // t2---t_swA---t_swB---tr (via CSMA)
    // t3---t_sw1---tr (simple path)

    // b2---b_swA---b_swB---br (via CSMA)
    // b3---b_sw1---br

    // Top LAN: create NetDevices for t2-t_swA-t_swB-tr (multi-switch path)
    CsmaHelper csmaTop;
    csmaTop.SetChannelAttribute("DataRate", StringValue(lanDataRate));
    csmaTop.SetChannelAttribute("Delay", StringValue(lanDelay));

    NetDeviceContainer t2_tswA = csmaTop.Install(NodeContainer(t2, t_swA));
    NetDeviceContainer tswA_tswB = csmaTop.Install(NodeContainer(t_swA, t_swB));
    NetDeviceContainer tswB_tr   = csmaTop.Install(NodeContainer(t_swB, tr));

    // t3---t_sw1---tr
    NetDeviceContainer t3_tsw1 = csmaTop.Install(NodeContainer(t3, t_sw1));
    NetDeviceContainer tsw1_tr = csmaTop.Install(NodeContainer(t_sw1, tr));

    // Bridge Devices on the switches
    // t_swA: [port to t2, port to t_swB]
    BridgeHelper bridge;
    NetDeviceContainer t_swA_ports;
    t_swA_ports.Add(t2_tswA.Get(1));
    t_swA_ports.Add(tswA_tswB.Get(0));
    bridge.Install(t_swA, t_swA_ports);

    // t_swB: [port to t_swA, port to tr]
    NetDeviceContainer t_swB_ports;
    t_swB_ports.Add(tswA_tswB.Get(1));
    t_swB_ports.Add(tswB_tr.Get(0));
    bridge.Install(t_swB, t_swB_ports);

    // t_sw1: [port to t3, port to tr]
    NetDeviceContainer t_sw1_ports;
    t_sw1_ports.Add(t3_tsw1.Get(1));
    t_sw1_ports.Add(tsw1_tr.Get(0));
    bridge.Install(t_sw1, t_sw1_ports);

    // Router tr will get IPs on both LAN (top) and WAN

    // Bottom LAN
    CsmaHelper csmaBottom;
    csmaBottom.SetChannelAttribute("DataRate", StringValue(lanDataRate));
    csmaBottom.SetChannelAttribute("Delay", StringValue(lanDelay));

    NetDeviceContainer b2_bswA = csmaBottom.Install(NodeContainer(b2, b_swA));
    NetDeviceContainer bswA_bswB = csmaBottom.Install(NodeContainer(b_swA, b_swB));
    NetDeviceContainer bswB_br   = csmaBottom.Install(NodeContainer(b_swB, br));

    NetDeviceContainer b3_bsw1 = csmaBottom.Install(NodeContainer(b3, b_sw1));
    NetDeviceContainer bsw1_br = csmaBottom.Install(NodeContainer(b_sw1, br));

    NetDeviceContainer b_swA_ports;
    b_swA_ports.Add(b2_bswA.Get(1));
    b_swA_ports.Add(bswA_bswB.Get(0));
    bridge.Install(b_swA, b_swA_ports);

    NetDeviceContainer b_swB_ports;
    b_swB_ports.Add(bswA_bswB.Get(1));
    b_swB_ports.Add(bswB_br.Get(0));
    bridge.Install(b_swB, b_swB_ports);

    NetDeviceContainer b_sw1_ports;
    b_sw1_ports.Add(b3_bsw1.Get(1));
    b_sw1_ports.Add(bsw1_br.Get(0));
    bridge.Install(b_sw1, b_sw1_ports);

    // Routers tr (top) and br (bottom) interconnect via WAN
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue(wanDataRate));
    p2p.SetChannelAttribute("Delay", StringValue(wanDelay));

    NetDeviceContainer wanDevices = p2p.Install(tr, br);

    // Assign IP addresses
    // Top LAN: 192.168.1.0/24
    // Bottom LAN: 192.168.2.0/24
    // WAN: 10.0.0.0/30

    // Top LAN bridge port collection (for assigning a single subnet)
    NetDeviceContainer topLanDevs;
    topLanDevs.Add(t2_tswA.Get(0));   // t2's port
    topLanDevs.Add(t3_tsw1.Get(0));   // t3's port
    topLanDevs.Add(tswB_tr.Get(1));   // tr's port for multi-switch
    topLanDevs.Add(tsw1_tr.Get(1));   // tr's port for t_sw1

    Ipv4AddressHelper ipv4Top;
    ipv4Top.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer ifTop = ipv4Top.Assign(topLanDevs);

    // Bottom LAN bridge port collection
    NetDeviceContainer bottomLanDevs;
    bottomLanDevs.Add(b2_bswA.Get(0));  // b2
    bottomLanDevs.Add(b3_bsw1.Get(0));  // b3
    bottomLanDevs.Add(bswB_br.Get(1));  // br (for b_swB multi-switch)
    bottomLanDevs.Add(bsw1_br.Get(1));  // br (for b_sw1)

    Ipv4AddressHelper ipv4Bottom;
    ipv4Bottom.SetBase("192.168.2.0", "255.255.255.0");
    Ipv4InterfaceContainer ifBottom = ipv4Bottom.Assign(bottomLanDevs);

    // WAN link: 10.0.0.0/30
    Ipv4AddressHelper ipv4Wan;
    ipv4Wan.SetBase("10.0.0.0", "255.255.255.252");
    Ipv4InterfaceContainer ifWan = ipv4Wan.Assign(wanDevices);

    // Enable global routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // UDP Echo applications configuration

    // t2 (in top LAN) is client, contacts b2 (in bottom LAN)
    // t3 (in top LAN) is server, receives from b3 (in bottom LAN)

    // Top client (t2) --> Bottom server (b2)
    uint16_t echoPort1 = 9;
    UdpEchoServerHelper echoServer1(echoPort1);
    ApplicationContainer serverApp1 = echoServer1.Install(b2);
    serverApp1.Start(Seconds(1.0));
    serverApp1.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient1(ifBottom.GetAddress(0), echoPort1); // b2 address
    echoClient1.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient1.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient1.SetAttribute("PacketSize", UintegerValue(64));
    ApplicationContainer clientApp1 = echoClient1.Install(t2);
    clientApp1.Start(Seconds(2.0));
    clientApp1.Stop(Seconds(10.0));

    // Bottom client (b3) --> Top server (t3)
    uint16_t echoPort2 = 10;
    UdpEchoServerHelper echoServer2(echoPort2);
    ApplicationContainer serverApp2 = echoServer2.Install(t3);
    serverApp2.Start(Seconds(1.0));
    serverApp2.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient2(ifTop.GetAddress(1), echoPort2); // t3 address
    echoClient2.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient2.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient2.SetAttribute("PacketSize", UintegerValue(64));
    ApplicationContainer clientApp2 = echoClient2.Install(b3);
    clientApp2.Start(Seconds(2.0));
    clientApp2.Stop(Seconds(10.0));

    // Set link enable pcap
    csmaTop.EnablePcapAll("lan-top");
    csmaBottom.EnablePcapAll("lan-bottom");
    p2p.EnablePcapAll("wan-link");

    Simulator::Stop(Seconds(11.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}