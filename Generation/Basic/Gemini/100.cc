#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/bridge-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("BridgedLansWanSimulation");

DataRate g_csmaDataRate = DataRate("100Mbps");
Time g_csmaDelay = NanoSeconds(6560); // 6.56 microseconds for typical CSMA cable
bool g_enablePcap = false;

int main(int argc, char* argv[])
{
    CommandLine cmd(__FILE__);
    cmd.AddValue("csmaDataRate", "CSMA link data rate (e.g., 10Mbps, 100Mbps)", g_csmaDataRate);
    cmd.AddValue("enablePcap", "Enable PCAP tracing on all devices", g_enablePcap);
    cmd.Parse(argc, argv);

    // Create nodes
    Ptr<Node> tr = CreateObject<Node>(); // Top Router
    Ptr<Node> br = CreateObject<Node>(); // Bottom Router

    Ptr<Node> ts1 = CreateObject<Node>(); // Top Switch 1 (central)
    Ptr<Node> ts2 = CreateObject<Node>(); // Top Switch 2
    Ptr<Node> ts3 = CreateObject<Node>(); // Top Switch 3
    Ptr<Node> ts4 = CreateObject<Node>(); // Top Switch 4
    Ptr<Node> t2 = CreateObject<Node>();  // Top Host 2
    Ptr<Node> t3 = CreateObject<Node>();  // Top Host 3

    Ptr<Node> bs1 = CreateObject<Node>(); // Bottom Switch 1 (central)
    Ptr<Node> bs2 = CreateObject<Node>(); // Bottom Switch 2
    Ptr<Node> bs3 = CreateObject<Node>(); // Bottom Switch 3
    Ptr<Node> bs4 = CreateObject<Node>(); // Bottom Switch 4
    Ptr<Node> bs5 = CreateObject<Node>(); // Bottom Switch 5
    Ptr<Node> b2 = CreateObject<Node>();  // Bottom Host 2
    Ptr<Node> b3 = CreateObject<Node>();  // Bottom Host 3

    // Install Internet Stack on all nodes
    InternetStackHelper internet;
    internet.Install(NodeContainer(tr, br, ts1, ts2, ts3, ts4, t2, t3, bs1, bs2, bs3, bs4, bs5, b2, b3));

    // WAN Link Configuration (PointToPoint)
    PointToPointHelper p2pHelper;
    p2pHelper.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2pHelper.SetChannelAttribute("Delay", StringValue("50ms"));

    NetDeviceContainer wanDevices = p2pHelper.Install(NodeContainer(tr, br));

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("76.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer wanInterfaces = ipv4.Assign(wanDevices);

    // CSMA Helper for LANs
    CsmaHelper csmaLanHelper;
    csmaLanHelper.SetChannelAttribute("Delay", TimeValue(g_csmaDelay));
    csmaLanHelper.SetDeviceAttribute("DataRate", DataRateValue(g_csmaDataRate));

    // Top LAN Setup (192.168.1.0/24)
    // Install BridgeNetDevice on each switch node
    Ptr<BridgeNetDevice> ts1_bridge = CreateObject<BridgeNetDevice>(); ts1->AddDevice(ts1_bridge);
    Ptr<BridgeNetDevice> ts2_bridge = CreateObject<BridgeNetDevice>(); ts2->AddDevice(ts2_bridge);
    Ptr<BridgeNetDevice> ts3_bridge = CreateObject<BridgeNetDevice>(); ts3->AddDevice(ts3_bridge);
    Ptr<BridgeNetDevice> ts4_bridge = CreateObject<BridgeNetDevice>(); ts4->AddDevice(ts4_bridge);

    // Connect router 'tr' to 'ts1' (central switch for top LAN)
    NetDeviceContainer tr_ts1_devs = csmaLanHelper.Install(NodeContainer(tr, ts1));
    ts1_bridge->AddBridgePort(tr_ts1_devs.Get(1)); // Device on ts1 is a port of ts1_bridge

    // Connect 'ts1' to 'ts2', 'ts3', 'ts4'
    NetDeviceContainer ts1_ts2_devs = csmaLanHelper.Install(NodeContainer(ts1, ts2));
    ts1_bridge->AddBridgePort(ts1_ts2_devs.Get(0));
    ts2_bridge->AddBridgePort(ts1_ts2_devs.Get(1));

    NetDeviceContainer ts1_ts3_devs = csmaLanHelper.Install(NodeContainer(ts1, ts3));
    ts1_bridge->AddBridgePort(ts1_ts3_devs.Get(0));
    ts3_bridge->AddBridgePort(ts1_ts3_devs.Get(1));

    NetDeviceContainer ts1_ts4_devs = csmaLanHelper.Install(NodeContainer(ts1, ts4));
    ts1_bridge->AddBridgePort(ts1_ts4_devs.Get(0));
    ts4_bridge->AddBridgePort(ts1_ts4_devs.Get(1));

    // Connect hosts 't2' and 't3'
    // t2 connects to ts2 (path involves multiple switches in LAN: t2-ts2-ts1-tr)
    NetDeviceContainer ts2_t2_devs = csmaLanHelper.Install(NodeContainer(ts2, t2));
    ts2_bridge->AddBridgePort(ts2_t2_devs.Get(0)); // Device on ts2 is a port of ts2_bridge

    // t3 connects directly to ts1 (path involves a single switch in LAN: t3-ts1-tr)
    NetDeviceContainer ts1_t3_devs = csmaLanHelper.Install(NodeContainer(ts1, t3));
    ts1_bridge->AddBridgePort(ts1_t3_devs.Get(0)); // Device on ts1 is a port of ts1_bridge

    // Assign IP addresses to Top LAN interfaces
    ipv4.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer trLanIf = ipv4.Assign(tr_ts1_devs.Get(0)); // Router's LAN interface
    Ipv4InterfaceContainer t2LanIf = ipv4.Assign(ts2_t2_devs.Get(1)); // Host t2's LAN interface
    Ipv4InterfaceContainer t3LanIf = ipv4.Assign(ts1_t3_devs.Get(1)); // Host t3's LAN interface


    // Bottom LAN Setup (192.168.2.0/24)
    // Install BridgeNetDevice on each switch node
    Ptr<BridgeNetDevice> bs1_bridge = CreateObject<BridgeNetDevice>(); bs1->AddDevice(bs1_bridge);
    Ptr<BridgeNetDevice> bs2_bridge = CreateObject<BridgeNetDevice>(); bs2->AddDevice(bs2_bridge);
    Ptr<BridgeNetDevice> bs3_bridge = CreateObject<BridgeNetDevice>(); bs3->AddDevice(bs3_bridge);
    Ptr<BridgeNetDevice> bs4_bridge = CreateObject<BridgeNetDevice>(); bs4->AddDevice(bs4_bridge);
    Ptr<BridgeNetDevice> bs5_bridge = CreateObject<BridgeNetDevice>(); bs5->AddDevice(bs5_bridge);

    // Connect router 'br' to 'bs1' (central switch for bottom LAN)
    NetDeviceContainer br_bs1_devs = csmaLanHelper.Install(NodeContainer(br, bs1));
    bs1_bridge->AddBridgePort(br_bs1_devs.Get(1)); // Device on bs1 is a port of bs1_bridge

    // Connect 'bs1' to 'bs2', 'bs3', 'bs4', 'bs5'
    NetDeviceContainer bs1_bs2_devs = csmaLanHelper.Install(NodeContainer(bs1, bs2));
    bs1_bridge->AddBridgePort(bs1_bs2_devs.Get(0));
    bs2_bridge->AddBridgePort(bs1_bs2_devs.Get(1));

    NetDeviceContainer bs1_bs3_devs = csmaLanHelper.Install(NodeContainer(bs1, bs3));
    bs1_bridge->AddBridgePort(bs1_bs3_devs.Get(0));
    bs3_bridge->AddBridgePort(bs1_bs3_devs.Get(1));

    NetDeviceContainer bs1_bs4_devs = csmaLanHelper.Install(NodeContainer(bs1, bs4));
    bs1_bridge->AddBridgePort(bs1_bs4_devs.Get(0));
    bs4_bridge->AddBridgePort(bs1_bs4_devs.Get(1));

    NetDeviceContainer bs1_bs5_devs = csmaLanHelper.Install(NodeContainer(bs1, bs5));
    bs1_bridge->AddBridgePort(bs1_bs5_devs.Get(0));
    bs5_bridge->AddBridgePort(bs1_bs5_devs.Get(1));

    // Connect hosts 'b2' and 'b3'
    // b2 connects to bs2 (path involves multiple switches in LAN: b2-bs2-bs1-br)
    NetDeviceContainer bs2_b2_devs = csmaLanHelper.Install(NodeContainer(bs2, b2));
    bs2_bridge->AddBridgePort(bs2_b2_devs.Get(0)); // Device on bs2 is a port of bs2_bridge

    // b3 connects directly to bs1 (path involves a single switch in LAN: b3-bs1-br)
    NetDeviceContainer bs1_b3_devs = csmaLanHelper.Install(NodeContainer(bs1, b3));
    bs1_bridge->AddBridgePort(bs1_b3_devs.Get(0)); // Device on bs1 is a port of bs1_bridge

    // Assign IP addresses to Bottom LAN interfaces
    ipv4.SetBase("192.168.2.0", "255.255.255.0");
    Ipv4InterfaceContainer brLanIf = ipv4.Assign(br_bs1_devs.Get(0)); // Router's LAN interface
    Ipv4InterfaceContainer b2LanIf = ipv4.Assign(bs2_b2_devs.Get(1)); // Host b2's LAN interface
    Ipv4InterfaceContainer b3LanIf = ipv4.Assign(bs1_b3_devs.Get(1)); // Host b3's LAN interface

    // Populate routing tables
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // UDP Echo Applications
    uint16_t echoPort = 9;

    // Servers (on b2 and b3)
    UdpEchoServerHelper echoServerB2(echoPort);
    ApplicationContainer serverAppsB2 = echoServerB2.Install(b2);
    serverAppsB2.Start(Seconds(1.0));
    serverAppsB2.Stop(Seconds(10.0));

    UdpEchoServerHelper echoServerB3(echoPort);
    ApplicationContainer serverAppsB3 = echoServerB3.Install(b3);
    serverAppsB3.Start(Seconds(1.0));
    serverAppsB3.Stop(Seconds(10.0));

    // Clients (on t2 and t3)
    // t2 -> b2 (multiple switches)
    UdpEchoClientHelper echoClientT2(b2LanIf.GetAddress(0), echoPort);
    echoClientT2.SetAttribute("MaxPackets", UintegerValue(10));
    echoClientT2.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClientT2.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientAppsT2 = echoClientT2.Install(t2);
    clientAppsT2.Start(Seconds(2.0));
    clientAppsT2.Stop(Seconds(9.0));

    // t3 -> b3 (single switch for LAN portion)
    UdpEchoClientHelper echoClientT3(b3LanIf.GetAddress(0), echoPort);
    echoClientT3.SetAttribute("MaxPackets", UintegerValue(10));
    echoClientT3.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClientT3.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientAppsT3 = echoClientT3.Install(t3);
    clientAppsT3.Start(Seconds(2.0));
    clientAppsT3.Stop(Seconds(9.0));

    // PCAP Tracing
    if (g_enablePcap)
    {
        p2pHelper.EnablePcapAll("wan-link");
        csmaLanHelper.EnablePcap("top-router", tr_ts1_devs.Get(0));
        csmaLanHelper.EnablePcap("bottom-router", br_bs1_devs.Get(0));
        csmaLanHelper.EnablePcap("t2-host", ts2_t2_devs.Get(1));
        csmaLanHelper.EnablePcap("t3-host", ts1_t3_devs.Get(1));
        csmaLanHelper.EnablePcap("b2-host", bs2_b2_devs.Get(1));
        csmaLanHelper.EnablePcap("b3-host", bs1_b3_devs.Get(1));
        // You can enable more PCAP traces on the channels if needed for debugging L2 forwarding
        // For instance, on the links between switches:
        csmaLanHelper.EnablePcap("ts1-ts2-link", ts1_ts2_devs.Get(0));
        csmaLanHelper.EnablePcap("bs1-bs2-link", bs1_bs2_devs.Get(0));
    }

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}