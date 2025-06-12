#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("LanWanBridgeSimulation");

int main(int argc, char *argv[]) {
    std::string csmaRate = "100Mbps";
    bool enablePcap = true;

    CommandLine cmd(__FILE__);
    cmd.AddValue("csmaRate", "CSMA link rate (100Mbps or 10Mbps)", csmaRate);
    cmd.AddValue("pcap", "Enable PCAP tracing", enablePcap);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);

    // Create nodes for top LAN: switches ts1-ts4 and endpoints t2, t3
    NodeContainer topSwitches;
    topSwitches.Create(4);
    NodeContainer topEnds;
    topEnds.Create(2);
    NodeContainer topRouter;
    topRouter.Create(1);

    // Create nodes for bottom LAN: switches bs1-bs5 and endpoints b2, b3
    NodeContainer bottomSwitches;
    bottomSwitches.Create(5);
    NodeContainer bottomEnds;
    bottomEnds.Create(2);
    NodeContainer bottomRouter;
    bottomRouter.Create(1);

    // Create WAN router connection
    NodeContainer routers;
    routers.Add(topRouter.Get(0));
    routers.Add(bottomRouter.Get(0));

    // CSMA Helper for LANs
    CsmaHelper csma;
    if (csmaRate == "100Mbps") {
        csma.SetChannelAttribute("DataRate", DataRateValue(DataRate("100Mbps")));
    } else {
        csma.SetChannelAttribute("DataRate", DataRateValue(DataRate("10Mbps")));
    }
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));

    // PointToPoint helper for WAN
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("50ms"));

    // Install Internet stack on all nodes
    InternetStackHelper internet;
    internet.InstallAll();

    // Top LAN connections

    // Connect switches in a chain: ts1 <-> ts2 <-> ts3 <-> ts4
    NetDeviceContainer topSwitchLinks[3];
    for (uint32_t i = 0; i < 3; ++i) {
        topSwitchLinks[i] = csma.Install(NodeContainer(topSwitches.Get(i), topSwitches.Get(i+1)));
    }

    // Connect t2 to ts1
    NetDeviceContainer t2ts1 = csma.Install(NodeContainer(topEnds.Get(0), topSwitches.Get(0)));

    // Connect t3 to ts4
    NetDeviceContainer t3ts4 = csma.Install(NodeContainer(topEnds.Get(1), topSwitches.Get(3)));

    // Connect router tr to ts2
    NetDeviceContainer trTs2 = csma.Install(NodeContainer(topRouter.Get(0), topSwitches.Get(1)));

    // Bottom LAN connections

    // Connect switches in a chain: bs1 <-> bs2 <-> bs3 <-> bs4 <-> bs5
    NetDeviceContainer bottomSwitchLinks[4];
    for (uint32_t i = 0; i < 4; ++i) {
        bottomSwitchLinks[i] = csma.Install(NodeContainer(bottomSwitches.Get(i), bottomSwitches.Get(i+1)));
    }

    // Connect b2 to bs3
    NetDeviceContainer b2bs3 = csma.Install(NodeContainer(bottomEnds.Get(0), bottomSwitches.Get(2)));

    // Connect b3 to bs1
    NetDeviceContainer b3bs1 = csma.Install(NodeContainer(bottomEnds.Get(1), bottomSwitches.Get(0)));

    // Connect router br to bs4
    NetDeviceContainer brBs4 = csma.Install(NodeContainer(bottomRouter.Get(0), bottomSwitches.Get(3)));

    // Connect routers via WAN
    NetDeviceContainer wanDevices = p2p.Install(routers);

    // Assign IP addresses
    Ipv4AddressHelper ip;

    // Top LAN subnet
    ip.SetBase("192.168.1.0", "255.255.255.0");
    ip.Assign(t2ts1);
    ip.Assign(t3ts4);
    ip.Assign(trTs2);

    // Bottom LAN subnet
    ip.SetBase("192.168.2.0", "255.255.255.0");
    ip.Assign(b2bs3);
    ip.Assign(b3bs1);
    ip.Assign(brBs4);

    // WAN subnet
    ip.SetBase("76.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer wanInterfaces = ip.Assign(wanDevices);

    // Set up global routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // UDP Echo applications on bottom LAN
    UdpEchoServerHelper echoServer(9);

    ApplicationContainer serverAppsB2 = echoServer.Install(bottomEnds.Get(0));
    serverAppsB2.Start(Seconds(1.0));
    serverAppsB2.Stop(Seconds(10.0));

    ApplicationContainer serverAppsB3 = echoServer.Install(bottomEnds.Get(1));
    serverAppsB3.Start(Seconds(1.0));
    serverAppsB3.Stop(Seconds(10.0));

    // UDP Echo clients from top LAN to bottom LAN
    UdpEchoClientHelper echoClientB2(wanInterfaces.GetAddress(1), 9);
    echoClientB2.SetAttribute("MaxPackets", UintegerValue(5));
    echoClientB2.SetAttribute("Interval", TimeValue(Seconds(1.)));
    echoClientB2.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientAppsB2 = echoClientB2.Install(topEnds.Get(0));
    clientAppsB2.Start(Seconds(2.0));
    clientAppsB2.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClientB3(wanInterfaces.GetAddress(1), 9);
    echoClientB3.SetAttribute("MaxPackets", UintegerValue(5));
    echoClientB3.SetAttribute("Interval", TimeValue(Seconds(1.)));
    echoClientB3.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientAppsB3 = echoClientB3.Install(topEnds.Get(1));
    clientAppsB3.Start(Seconds(3.0));
    clientAppsB3.Stop(Seconds(10.0));

    // Enable pcap traces if requested
    if (enablePcap) {
        csma.EnablePcapAll("lan_wan_bridge", false);
        p2p.EnablePcapAll("wan_link", false);
    }

    Simulator::Run();
    Simulator::Destroy();
    return 0;
}