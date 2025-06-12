#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/bridge-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TwoLanWithBridgedSwitches");

int main(int argc, char *argv[])
{
    std::string csmaRateFast = "100Mbps";
    std::string csmaRateSlow = "10Mbps";
    std::string csmaRate = csmaRateFast;
    std::string csmaDelay = "6560ns";
    std::string wanDataRate = "5Mbps";
    std::string wanDelay = "50ms";

    CommandLine cmd;
    cmd.AddValue("csmaRate", "Ethernet LAN data rate: \"100Mbps\" or \"10Mbps\"", csmaRate);
    cmd.AddValue("wanDataRate", "WAN data rate", wanDataRate);
    cmd.AddValue("wanDelay", "WAN delay", wanDelay);
    cmd.Parse(argc, argv);

    if (csmaRate != csmaRateFast && csmaRate != csmaRateSlow) {
        NS_ABORT_MSG("Invalid csmaRate: use '100Mbps' or '10Mbps'");
    }

    // Create all nodes needed
    // Top LAN: t2 (client, multi-switch), t3 (server, single-switch), tr (router/gw)
    // Bottom LAN: b2 (server, multi-switch), b3 (client, single-switch), br (router/gw)
    Ptr<Node> t2 = CreateObject<Node>();
    Ptr<Node> t3 = CreateObject<Node>();
    Ptr<Node> tr = CreateObject<Node>();
    Ptr<Node> b2 = CreateObject<Node>();
    Ptr<Node> b3 = CreateObject<Node>();
    Ptr<Node> br = CreateObject<Node>();

    // Switches
    Ptr<Node> tsw1 = CreateObject<Node>();
    Ptr<Node> tsw2 = CreateObject<Node>();
    Ptr<Node> bsw1 = CreateObject<Node>();
    Ptr<Node> bsw2 = CreateObject<Node>();

    // Container for convenience
    NodeContainer allNodes;
    allNodes.Add(t2);
    allNodes.Add(t3);
    allNodes.Add(tr);
    allNodes.Add(b2);
    allNodes.Add(b3);
    allNodes.Add(br);
    allNodes.Add(tsw1);
    allNodes.Add(tsw2);
    allNodes.Add(bsw1);
    allNodes.Add(bsw2);

    InternetStackHelper stack;
    stack.Install(t2);
    stack.Install(t3);
    stack.Install(tr);
    stack.Install(b2);
    stack.Install(b3);
    stack.Install(br);

    // ********** Top LAN (192.168.1.0/24) **********

    // t2 connects to tr via tsw2 <-> tsw1 chain (multi-switch path)
    // t3 connects to tr via tsw1 (single-switch path)

    // t2 <-> tsw2
    CsmaHelper csmaLanTop1;
    csmaLanTop1.SetChannelAttribute("DataRate", StringValue(csmaRate));
    csmaLanTop1.SetChannelAttribute("Delay", StringValue(csmaDelay));
    NetDeviceContainer t2_tsw2 = csmaLanTop1.Install(NodeContainer(t2, tsw2));

    // tsw2 <-> tsw1
    CsmaHelper csmaLanTop2;
    csmaLanTop2.SetChannelAttribute("DataRate", StringValue(csmaRate));
    csmaLanTop2.SetChannelAttribute("Delay", StringValue(csmaDelay));
    NetDeviceContainer tsw2_tsw1 = csmaLanTop2.Install(NodeContainer(tsw2, tsw1));

    // tsw1 <-> tr
    CsmaHelper csmaLanTop3;
    csmaLanTop3.SetChannelAttribute("DataRate", StringValue(csmaRate));
    csmaLanTop3.SetChannelAttribute("Delay", StringValue(csmaDelay));
    NetDeviceContainer tsw1_tr = csmaLanTop3.Install(NodeContainer(tsw1, tr));

    // t3 <-> tsw1
    CsmaHelper csmaLanTop4;
    csmaLanTop4.SetChannelAttribute("DataRate", StringValue(csmaRate));
    csmaLanTop4.SetChannelAttribute("Delay", StringValue(csmaDelay));
    NetDeviceContainer t3_tsw1 = csmaLanTop4.Install(NodeContainer(t3, tsw1));

    // Bridge ports configuration (tsw2 & tsw1 are switches)
    // tsw2: 2 ports (t2, tsw1)
    NetDeviceContainer tsw2_ports;
    tsw2_ports.Add(t2_tsw2.Get(1));    // port to t2
    tsw2_ports.Add(tsw2_tsw1.Get(0));  // port to tsw1

    BridgeHelper bridge;
    bridge.Install(tsw2, tsw2_ports);

    // tsw1: up to 3 ports (tsw2, tr, t3)
    NetDeviceContainer tsw1_ports;
    tsw1_ports.Add(tsw2_tsw1.Get(1)); // port to tsw2
    tsw1_ports.Add(tsw1_tr.Get(0));   // port to tr
    tsw1_ports.Add(t3_tsw1.Get(1));   // port to t3

    bridge.Install(tsw1, tsw1_ports);

    // ********** Bottom LAN (192.168.2.0/24) **********

    // b2 connects to br via bsw2 <-> bsw1 chain (multi-switch path)
    // b3 connects to br via bsw1 (single-switch path)

    // b2 <-> bsw2
    CsmaHelper csmaLanBot1;
    csmaLanBot1.SetChannelAttribute("DataRate", StringValue(csmaRate));
    csmaLanBot1.SetChannelAttribute("Delay", StringValue(csmaDelay));
    NetDeviceContainer b2_bsw2 = csmaLanBot1.Install(NodeContainer(b2, bsw2));

    // bsw2 <-> bsw1
    CsmaHelper csmaLanBot2;
    csmaLanBot2.SetChannelAttribute("DataRate", StringValue(csmaRate));
    csmaLanBot2.SetChannelAttribute("Delay", StringValue(csmaDelay));
    NetDeviceContainer bsw2_bsw1 = csmaLanBot2.Install(NodeContainer(bsw2, bsw1));

    // bsw1 <-> br
    CsmaHelper csmaLanBot3;
    csmaLanBot3.SetChannelAttribute("DataRate", StringValue(csmaRate));
    csmaLanBot3.SetChannelAttribute("Delay", StringValue(csmaDelay));
    NetDeviceContainer bsw1_br = csmaLanBot3.Install(NodeContainer(bsw1, br));

    // b3 <-> bsw1
    CsmaHelper csmaLanBot4;
    csmaLanBot4.SetChannelAttribute("DataRate", StringValue(csmaRate));
    csmaLanBot4.SetChannelAttribute("Delay", StringValue(csmaDelay));
    NetDeviceContainer b3_bsw1 = csmaLanBot4.Install(NodeContainer(b3, bsw1));

    // bsw2: 2 ports (b2, bsw1)
    NetDeviceContainer bsw2_ports;
    bsw2_ports.Add(b2_bsw2.Get(1));     // port to b2
    bsw2_ports.Add(bsw2_bsw1.Get(0));   // port to bsw1

    bridge.Install(bsw2, bsw2_ports);

    // bsw1: up to 3 ports (bsw2, br, b3)
    NetDeviceContainer bsw1_ports;
    bsw1_ports.Add(bsw2_bsw1.Get(1));   // port to bsw2
    bsw1_ports.Add(bsw1_br.Get(0));     // port to br
    bsw1_ports.Add(b3_bsw1.Get(1));     // port to b3

    bridge.Install(bsw1, bsw1_ports);

    // ********** WAN link (tr <-> br) **********
    PointToPointHelper p2pWan;
    p2pWan.SetDeviceAttribute("DataRate", StringValue(wanDataRate));
    p2pWan.SetChannelAttribute("Delay", StringValue(wanDelay));
    NetDeviceContainer tr_br_wan = p2pWan.Install(NodeContainer(tr, br));

    // ********** IP Addressing **********
    Ipv4AddressHelper ipv4TopLan;
    ipv4TopLan.SetBase("192.168.1.0", "255.255.255.0");

    // LAN interface/netdevices for IP:
    // t2: device 0
    // t3: device 0
    // tr: device 0 (top LAN), device 1 (WAN)
    NetDeviceContainer topLanDevices;
    topLanDevices.Add(t2_tsw2.Get(0)); // t2's LAN device
    topLanDevices.Add(t3_tsw1.Get(0)); // t3's LAN device
    topLanDevices.Add(tsw1_tr.Get(1)); // tr's LAN device

    Ipv4InterfaceContainer topLanInterfaces = ipv4TopLan.Assign(topLanDevices);

    // WAN
    Ipv4AddressHelper ipv4Wan;
    ipv4Wan.SetBase("10.10.10.0", "255.255.255.0");
    Ipv4InterfaceContainer wanIf = ipv4Wan.Assign(tr_br_wan);

    // Bottom LAN
    Ipv4AddressHelper ipv4BotLan;
    ipv4BotLan.SetBase("192.168.2.0", "255.255.255.0");

    NetDeviceContainer botLanDevices;
    botLanDevices.Add(b2_bsw2.Get(0)); // b2's LAN device
    botLanDevices.Add(b3_bsw1.Get(0)); // b3's LAN device
    botLanDevices.Add(bsw1_br.Get(1)); // br's LAN device

    Ipv4InterfaceContainer botLanInterfaces = ipv4BotLan.Assign(botLanDevices);

    // ********** Set Default Gateways **********
    Ipv4StaticRoutingHelper staticRouting;
    // Top LAN
    Ptr<Ipv4> t2Ipv4 = t2->GetObject<Ipv4>();
    Ptr<Ipv4StaticRouting> t2Static = staticRouting.GetStaticRouting(t2Ipv4);
    t2Static->SetDefaultRoute(topLanInterfaces.GetAddress(2), 1); // via tr (tr's LAN addr)

    Ptr<Ipv4> t3Ipv4 = t3->GetObject<Ipv4>();
    Ptr<Ipv4StaticRouting> t3Static = staticRouting.GetStaticRouting(t3Ipv4);
    t3Static->SetDefaultRoute(topLanInterfaces.GetAddress(2), 1);

    // Bottom LAN
    Ptr<Ipv4> b2Ipv4 = b2->GetObject<Ipv4>();
    Ptr<Ipv4StaticRouting> b2Static = staticRouting.GetStaticRouting(b2Ipv4);
    b2Static->SetDefaultRoute(botLanInterfaces.GetAddress(2), 1); // via br (br's LAN addr)

    Ptr<Ipv4> b3Ipv4 = b3->GetObject<Ipv4>();
    Ptr<Ipv4StaticRouting> b3Static = staticRouting.GetStaticRouting(b3Ipv4);
    b3Static->SetDefaultRoute(botLanInterfaces.GetAddress(2), 1);

    // Enable IP forwarding on routers
    Ptr<Ipv4> trIpv4 = tr->GetObject<Ipv4>();
    trIpv4->SetAttribute("IpForward", BooleanValue(true));
    Ptr<Ipv4> brIpv4 = br->GetObject<Ipv4>();
    brIpv4->SetAttribute("IpForward", BooleanValue(true));

    // Add static routes for WAN on the routers
    Ptr<Ipv4StaticRouting> trStatic = staticRouting.GetStaticRouting(trIpv4);
    Ptr<Ipv4StaticRouting> brStatic = staticRouting.GetStaticRouting(brIpv4);

    // Top LAN: 192.168.1.0/24 via tr's LAN device
    trStatic->AddNetworkRouteTo(Ipv4Address("192.168.1.0"), Ipv4Mask("255.255.255.0"), tr->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal(), 1);
    // Bottom LAN: 192.168.2.0/24 via br's LAN device
    brStatic->AddNetworkRouteTo(Ipv4Address("192.168.2.0"), Ipv4Mask("255.255.255.0"), br->GetObject<Ipv4>()->GetAddress(1,0).GetLocal(), 1);

    // Top router: route to bottom LAN via WAN
    trStatic->AddNetworkRouteTo(Ipv4Address("192.168.2.0"), Ipv4Mask("255.255.255.0"),
                                wanIf.GetAddress(1), 2);

    // Bottom router: route to top LAN via WAN
    brStatic->AddNetworkRouteTo(Ipv4Address("192.168.1.0"), Ipv4Mask("255.255.255.0"),
                                wanIf.GetAddress(0), 2);

    // ********** Applications: UDP Echo Servers/Clients **********

    // UDP echo server on t3 (top LAN)
    uint16_t echoPortT3 = 7000;
    UdpEchoServerHelper echoServerT3(echoPortT3);
    ApplicationContainer serverAppT3 = echoServerT3.Install(t3);
    serverAppT3.Start(Seconds(1.0));
    serverAppT3.Stop(Seconds(10.0));

    // UDP echo client on b3 (bottom LAN), sends to t3
    UdpEchoClientHelper echoClientB3(topLanInterfaces.GetAddress(1), echoPortT3); // t3's IP
    echoClientB3.SetAttribute("MaxPackets", UintegerValue(5));
    echoClientB3.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClientB3.SetAttribute("PacketSize", UintegerValue(256));
    ApplicationContainer clientAppB3 = echoClientB3.Install(b3);
    clientAppB3.Start(Seconds(2.0));
    clientAppB3.Stop(Seconds(10.0));

    // UDP echo server on b2 (bottom LAN)
    uint16_t echoPortB2 = 8000;
    UdpEchoServerHelper echoServerB2(echoPortB2);
    ApplicationContainer serverAppB2 = echoServerB2.Install(b2);
    serverAppB2.Start(Seconds(1.0));
    serverAppB2.Stop(Seconds(10.0));

    // UDP echo client on t2 (top LAN), sends to b2
    UdpEchoClientHelper echoClientT2(botLanInterfaces.GetAddress(0), echoPortB2); // b2's IP
    echoClientT2.SetAttribute("MaxPackets", UintegerValue(5));
    echoClientT2.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClientT2.SetAttribute("PacketSize", UintegerValue(256));
    ApplicationContainer clientAppT2 = echoClientT2.Install(t2);
    clientAppT2.Start(Seconds(2.0));
    clientAppT2.Stop(Seconds(10.0));

    // ********** Enable packet capture and logging **********
    CsmaHelper::EnablePcapAll("lan-switch");
    p2pWan.EnablePcapAll("wan");

    Simulator::Stop(Seconds(11.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}