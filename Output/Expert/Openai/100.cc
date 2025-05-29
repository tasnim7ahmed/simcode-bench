#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/internet-module.h"
#include "ns3/bridge-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int
main(int argc, char *argv[])
{
    std::string csmaRate = "100Mbps";
    CommandLine cmd;
    cmd.AddValue("csmaRate", "Data rate for CSMA links: 100Mbps or 10Mbps", csmaRate);
    cmd.Parse(argc, argv);

    // Top LAN
    NodeContainer topHosts;
    topHosts.Create(2); // t2, t3

    NodeContainer topSwitches;
    topSwitches.Create(4); // ts1, ts2, ts3, ts4

    Ptr<Node> t2 = topHosts.Get(0), t3 = topHosts.Get(1);
    Ptr<Node> ts1 = topSwitches.Get(0), ts2 = topSwitches.Get(1);
    Ptr<Node> ts3 = topSwitches.Get(2), ts4 = topSwitches.Get(3);

    Ptr<Node> tr = CreateObject<Node>(); // Top router

    // Bottom LAN
    NodeContainer bottomHosts;
    bottomHosts.Create(2); // b2, b3

    NodeContainer bottomSwitches;
    bottomSwitches.Create(5); // bs1, bs2, bs3, bs4, bs5

    Ptr<Node> b2 = bottomHosts.Get(0), b3 = bottomHosts.Get(1);
    Ptr<Node> bs1 = bottomSwitches.Get(0), bs2 = bottomSwitches.Get(1);
    Ptr<Node> bs3 = bottomSwitches.Get(2), bs4 = bottomSwitches.Get(3), bs5 = bottomSwitches.Get(4);

    Ptr<Node> br = CreateObject<Node>(); // Bottom router

    NodeContainer wanRouters = NodeContainer(tr, br);

    // WAN: tr <-> br
    PointToPointHelper p2pWan;
    p2pWan.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2pWan.SetChannelAttribute("Delay", StringValue("50ms"));

    NetDeviceContainer wanDevices = p2pWan.Install(tr, br);

    // Install Internet on all nodes
    NodeContainer allNodes;
    allNodes.Add(topHosts);
    allNodes.Add(bottomHosts);
    allNodes.Add(tr);
    allNodes.Add(br);

    InternetStackHelper stack;
    stack.Install(allNodes);

    // Handy function for Bridge+CSMA switch setup
    auto SetupSwitch = [&csmaRate](Ptr<Node> switchNode, NodeContainer &ports, NetDeviceContainer &allDevices, std::string pcapBase = "") {
        CsmaHelper csma;
        csma.SetChannelAttribute("DataRate", StringValue(csmaRate));
        csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));

        NetDeviceContainer devs;
        for (uint32_t i = 0; i < ports.GetN(); ++i)
            devs.Add(csma.Install(NodeContainer(switchNode, ports.Get(i)))[0]); // [0]: switch side
        BridgeHelper bridge;
        NetDeviceContainer brdev;
        brdev = bridge.Install(switchNode, devs);
        allDevices.Add(devs);

        if (!pcapBase.empty()) {
            csma.EnablePcap(pcapBase, devs, true);
        }
        return brdev;
    };

    // Top LAN CSMA setup
    // Connections:
    // t2 - ts1, ts1-ts2-ts3-t3
    // tr - ts4
    // We'll do: t2<->ts1<->ts2<->ts3<->t3, tr<->ts4; connect ts1 to ts2, ts2 to ts3, etc.

    CsmaHelper topCsma;
    topCsma.SetChannelAttribute("DataRate", StringValue(csmaRate));
    topCsma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));

    // Link t2 <-> ts1
    NetDeviceContainer t2Dev = topCsma.Install(NodeContainer(t2, ts1));

    // ts1 <-> ts2
    NetDeviceContainer ts1ts2 = topCsma.Install(NodeContainer(ts1, ts2));
    // ts2 <-> ts3
    NetDeviceContainer ts2ts3 = topCsma.Install(NodeContainer(ts2, ts3));
    // ts3 <-> t3
    NetDeviceContainer ts3t3 = topCsma.Install(NodeContainer(ts3, t3));

    // ts4 <-> tr
    NetDeviceContainer ts4tr = topCsma.Install(NodeContainer(ts4, tr));

    // Form multi-switch top LAN using bridges
    NetDeviceContainer topTs1Ports;
    topTs1Ports.Add(t2Dev.Get(1));     // connects to t2
    topTs1Ports.Add(ts1ts2.Get(0));    // connects to ts2

    NetDeviceContainer topTs2Ports;
    topTs2Ports.Add(ts1ts2.Get(1));    // connects to ts1
    topTs2Ports.Add(ts2ts3.Get(0));    // connects to ts3

    NetDeviceContainer topTs3Ports;
    topTs3Ports.Add(ts2ts3.Get(1));    // connects to ts2
    topTs3Ports.Add(ts3t3.Get(0));     // connects to t3

    NetDeviceContainer topTs4Ports;
    topTs4Ports.Add(ts4tr.Get(0));     // switch side

    BridgeHelper bridge;
    // Setup bridges on switches
    bridge.Install(ts1, topTs1Ports);
    bridge.Install(ts2, topTs2Ports);
    bridge.Install(ts3, topTs3Ports);
    bridge.Install(ts4, topTs4Ports);

    // Top LAN: Aggregate all L2 devices for IP assignment
    NetDeviceContainer topLanDevices;
    topLanDevices.Add(t2Dev.Get(0)); // t2 user side
    topLanDevices.Add(ts3t3.Get(1)); // t3 user side
    topLanDevices.Add(ts4tr.Get(1)); // router side

    // Bottom LAN (more switches)
    // Arrange: b2 - bs1 - bs2 - bs3 - bs4 - b3; br links to bs5, individuals to switches
    CsmaHelper bottomCsma;
    bottomCsma.SetChannelAttribute("DataRate", StringValue(csmaRate));
    bottomCsma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));

    NetDeviceContainer b2Dev = bottomCsma.Install(NodeContainer(b2, bs1));
    NetDeviceContainer bs1bs2 = bottomCsma.Install(NodeContainer(bs1, bs2));
    NetDeviceContainer bs2bs3 = bottomCsma.Install(NodeContainer(bs2, bs3));
    NetDeviceContainer bs3bs4 = bottomCsma.Install(NodeContainer(bs3, bs4));
    NetDeviceContainer bs4b3 = bottomCsma.Install(NodeContainer(bs4, b3));
    NetDeviceContainer bs5br = bottomCsma.Install(NodeContainer(bs5, br));

    // Ports for bridges
    NetDeviceContainer bs1Ports;
    bs1Ports.Add(b2Dev.Get(1));
    bs1Ports.Add(bs1bs2.Get(0));

    NetDeviceContainer bs2Ports;
    bs2Ports.Add(bs1bs2.Get(1));
    bs2Ports.Add(bs2bs3.Get(0));

    NetDeviceContainer bs3Ports;
    bs3Ports.Add(bs2bs3.Get(1));
    bs3Ports.Add(bs3bs4.Get(0));

    NetDeviceContainer bs4Ports;
    bs4Ports.Add(bs3bs4.Get(1));
    bs4Ports.Add(bs4b3.Get(0));

    NetDeviceContainer bs5Ports;
    bs5Ports.Add(bs5br.Get(0));

    bridge.Install(bs1, bs1Ports);
    bridge.Install(bs2, bs2Ports);
    bridge.Install(bs3, bs3Ports);
    bridge.Install(bs4, bs4Ports);
    bridge.Install(bs5, bs5Ports);

    NetDeviceContainer bottomLanDevices;
    bottomLanDevices.Add(b2Dev.Get(0));        // b2 side
    bottomLanDevices.Add(bs4b3.Get(1));        // b3 side
    bottomLanDevices.Add(bs5br.Get(1));        // router side

    // IP Assignment
    Ipv4AddressHelper ipTop, ipBottom, ipWan;
    ipTop.SetBase("192.168.1.0", "255.255.255.0");
    ipBottom.SetBase("192.168.2.0", "255.255.255.0");
    ipWan.SetBase("76.1.1.0", "255.255.255.0");

    Ipv4InterfaceContainer topLanIfs = ipTop.Assign(topLanDevices);
    Ipv4InterfaceContainer bottomLanIfs = ipBottom.Assign(bottomLanDevices);
    Ipv4InterfaceContainer wanIfs = ipWan.Assign(wanDevices);

    // Set static routing
    // Top router
    Ptr<Ipv4StaticRouting> trStatic = Ipv4RoutingHelper::GetRouting<Ipv4StaticRouting>(tr->GetObject<Ipv4>());
    trStatic->AddNetworkRouteTo(Ipv4Address("192.168.2.0"), Ipv4Mask("255.255.255.0"),
                                wanIfs.GetAddress(1), 1);
    // Bottom router
    Ptr<Ipv4StaticRouting> brStatic = Ipv4RoutingHelper::GetRouting<Ipv4StaticRouting>(br->GetObject<Ipv4>());
    brStatic->AddNetworkRouteTo(Ipv4Address("192.168.1.0"), Ipv4Mask("255.255.255.0"),
                                wanIfs.GetAddress(0), 1);

    // Set host default routes
    Ipv4Address trTopLanIp = topLanIfs.GetAddress(2);      // tr's top LAN interface
    Ipv4Address brBottomLanIp = bottomLanIfs.GetAddress(2); // br's bottom LAN interface

    Ptr<Ipv4StaticRouting> t2Routing = Ipv4RoutingHelper::GetRouting<Ipv4StaticRouting>(t2->GetObject<Ipv4>());
    t2Routing->SetDefaultRoute(trTopLanIp, 1);
    Ptr<Ipv4StaticRouting> t3Routing = Ipv4RoutingHelper::GetRouting<Ipv4StaticRouting>(t3->GetObject<Ipv4>());
    t3Routing->SetDefaultRoute(trTopLanIp, 1);
    Ptr<Ipv4StaticRouting> b2Routing = Ipv4RoutingHelper::GetRouting<Ipv4StaticRouting>(b2->GetObject<Ipv4>());
    b2Routing->SetDefaultRoute(brBottomLanIp, 1);
    Ptr<Ipv4StaticRouting> b3Routing = Ipv4RoutingHelper::GetRouting<Ipv4StaticRouting>(b3->GetObject<Ipv4>());
    b3Routing->SetDefaultRoute(brBottomLanIp, 1);

    // UDP echo servers (one per endpoint)
    uint16_t portB2 = 9, portB3 = 10;
    UdpEchoServerHelper echoServerB2(portB2);
    ApplicationContainer srvB2Apps = echoServerB2.Install(b2);
    srvB2Apps.Start(Seconds(1));
    srvB2Apps.Stop(Seconds(15));

    UdpEchoServerHelper echoServerB3(portB3);
    ApplicationContainer srvB3Apps = echoServerB3.Install(b3);
    srvB3Apps.Start(Seconds(1));
    srvB3Apps.Stop(Seconds(15));

    // UDP echo clients at t2->b2 (traverse all top/bottom switches) & t3->b3 (single switch on each side)
    UdpEchoClientHelper echoClientB2(bottomLanIfs.GetAddress(0), portB2);
    echoClientB2.SetAttribute("MaxPackets", UintegerValue(4));
    echoClientB2.SetAttribute("Interval", TimeValue(Seconds(1.)));
    echoClientB2.SetAttribute("PacketSize", UintegerValue(64));
    ApplicationContainer cliT2Apps = echoClientB2.Install(t2);
    cliT2Apps.Start(Seconds(2));
    cliT2Apps.Stop(Seconds(10));

    UdpEchoClientHelper echoClientB3(bottomLanIfs.GetAddress(1), portB3);
    echoClientB3.SetAttribute("MaxPackets", UintegerValue(4));
    echoClientB3.SetAttribute("Interval", TimeValue(Seconds(1.)));
    echoClientB3.SetAttribute("PacketSize", UintegerValue(64));
    ApplicationContainer cliT3Apps = echoClientB3.Install(t3);
    cliT3Apps.Start(Seconds(2.5));
    cliT3Apps.Stop(Seconds(10));

    // Enable PCAP
    p2pWan.EnablePcapAll("wan-link", true);
    topCsma.EnablePcap("toplan-t2", t2Dev.Get(0), true);
    topCsma.EnablePcap("toplan-t3", ts3t3.Get(1), true);
    topCsma.EnablePcap("toplan-router", ts4tr.Get(1), true);
    bottomCsma.EnablePcap("bottomlan-b2", b2Dev.Get(0), true);
    bottomCsma.EnablePcap("bottomlan-b3", bs4b3.Get(1), true);
    bottomCsma.EnablePcap("bottomlan-router", bs5br.Get(1), true);

    Simulator::Stop(Seconds(16));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}