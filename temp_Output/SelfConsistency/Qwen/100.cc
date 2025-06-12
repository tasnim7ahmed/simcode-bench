#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("LanWanBridgingSimulation");

int main(int argc, char *argv[])
{
    std::string csmaRate = "100Mbps";

    CommandLine cmd(__FILE__);
    cmd.AddValue("csmaRate", "CSMA link data rate (e.g., 10Mbps or 100Mbps)", csmaRate);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create nodes for top LAN: ts1-ts4 are switches, t2 and t3 are endpoints
    NodeContainer topSwitches;
    topSwitches.Create(4); // ts1, ts2, ts3, ts4

    NodeContainer topHosts;
    topHosts.Create(2); // t2, t3

    // Create router tr between top LAN and WAN
    Ptr<Node> tr = CreateObject<Node>();

    // Bottom LAN: bs1-bs5 are switches, b2 and b3 are hosts
    NodeContainer bottomSwitches;
    bottomSwitches.Create(5); // bs1 to bs5

    NodeContainer bottomHosts;
    bottomHosts.Create(2); // b2, b3

    // Create router br between WAN and bottom LAN
    Ptr<Node> br = CreateObject<Node>();

    // Install Internet stack on all routers and hosts
    InternetStackHelper internet;
    internet.Install(topHosts);
    internet.Install(bottomHosts);
    internet.Install(tr);
    internet.Install(br);

    // Install CSMA devices for switches in both LANs
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", DataRateValue(DataRate(csmaRate)));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));

    // Connect top switches together using CSMA
    NetDeviceContainer topSwitchDevices[6]; // 4 switches need 6 links in full mesh
    for (int i = 0; i < 4; ++i)
    {
        for (int j = i + 1; j < 4; ++j)
        {
            NodeContainer pair = NodeContainer(topSwitches.Get(i), topSwitches.Get(j));
            topSwitchDevices[i].Add(csma.Install(pair).Get(0));
            topSwitchDevices[j].Add(csma.Install(pair).Get(1));
        }
    }

    // Connect top hosts to switches
    NetDeviceContainer topHostDevices;
    topHostDevices.Add(csma.Install(NodeContainer(topSwitches.Get(0), topHosts.Get(0))).Get(1)); // t2 connected to ts1
    topHostDevices.Add(csma.Install(NodeContainer(topSwitches.Get(3), topHosts.Get(1))).Get(1)); // t3 connected to ts4

    // Connect top router tr to top switches
    NetDeviceContainer trTopDevices;
    for (uint32_t i = 0; i < 4; ++i)
    {
        trTopDevices.Add(csma.Install(NodeContainer(tr, topSwitches.Get(i))).Get(0));
    }

    // Connect bottom switches together using CSMA
    NetDeviceContainer bottomSwitchDevices[10]; // 5 switches require 10 links for full mesh
    for (int i = 0; i < 5; ++i)
    {
        for (int j = i + 1; j < 5; ++j)
        {
            NodeContainer pair = NodeContainer(bottomSwitches.Get(i), bottomSwitches.Get(j));
            bottomSwitchDevices[i].Add(csma.Install(pair).Get(0));
            bottomSwitchDevices[j].Add(csma.Install(pair).Get(1));
        }
    }

    // Connect bottom hosts to switches
    NetDeviceContainer bottomHostDevices;
    bottomHostDevices.Add(csma.Install(NodeContainer(bottomSwitches.Get(0), bottomHosts.Get(0))).Get(1)); // b2 connected to bs1
    bottomHostDevices.Add(csma.Install(NodeContainer(bottomSwitches.Get(2), bottomHosts.Get(1))).Get(1)); // b3 connected to bs3 through one switch

    // Connect bottom router br to bottom switches
    NetDeviceContainer brBottomDevices;
    for (uint32_t i = 0; i < 5; ++i)
    {
        brBottomDevices.Add(csma.Install(NodeContainer(br, bottomSwitches.Get(i))).Get(0));
    }

    // Set up WAN link between tr and br
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("50ms"));

    NetDeviceContainer wanDevices = p2p.Install(NodeContainer(tr, br));

    // Assign IP addresses
    Ipv4AddressHelper ip;

    ip.SetBase("192.168.1.0", "255.255.255.0");
    ip.Assign(topHostDevices);
    for (uint32_t i = 0; i < trTopDevices.GetN(); ++i)
    {
        ip.NewNetwork();
        ip.Assign(trTopDevices.Get(i));
    }

    ip.SetBase("192.168.2.0", "255.255.255.0");
    ip.Assign(bottomHostDevices);
    for (uint32_t i = 0; i < brBottomDevices.GetN(); ++i)
    {
        ip.NewNetwork();
        ip.Assign(brBottomDevices.Get(i));
    }

    ip.SetBase("76.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer wanInterfaces = ip.Assign(wanDevices);

    // Set routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Setup applications
    uint16_t port = 9;

    // Bottom servers: b2 and b3 listen on port 9
    UdpEchoServerHelper echoServer(port);

    ApplicationContainer serverApps = echoServer.Install(bottomHosts.Get(0)); // b2 as server
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    serverApps = echoServer.Install(bottomHosts.Get(1)); // b3 as server
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // Top clients: t2 sends to b2, t3 sends to b3
    UdpEchoClientHelper echoClientHelper(bbottomHosts.Get(0)->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal(), port);
    echoClientHelper.SetAttribute("MaxPackets", UintegerValue(5));
    echoClientHelper.SetAttribute("Interval", TimeValue(Seconds(1.)));
    echoClientHelper.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = echoClientHelper.Install(topHosts.Get(0)); // t2 -> b2
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    echoClientHelper = UdpEchoClientHelper(bottomHosts.Get(1)->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal(), port);
    echoClientHelper.SetAttribute("MaxPackets", UintegerValue(5));
    echoClientHelper.SetAttribute("Interval", TimeValue(Seconds(1.)));
    echoClientHelper.SetAttribute("PacketSize", UintegerValue(1024));

    clientApps = echoClientHelper.Install(topHosts.Get(1)); // t3 -> b3
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Enable PCAP traces
    csma.EnablePcapAll("lan_wan_bridge", false);
    p2p.EnablePcapAll("wan_link", false);

    Simulator::Run();
    Simulator::Destroy();
    return 0;
}