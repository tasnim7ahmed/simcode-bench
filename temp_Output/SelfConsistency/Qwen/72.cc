#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/bridge-helper.h"
#include "ns3/global-router-interface.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TwoLanWanTopology");

int main(int argc, char *argv[]) {
    std::string lanLinkType = "Csma"; // Csma or PointToPoint (though CSMA is expected for LAN)
    std::string csmaRate = "100Mbps";
    uint32_t csmaDelay = 2; // in ms
    std::string wanRate = "5Mbps";
    std::string wanDelay = "50ms";

    CommandLine cmd(__FILE__);
    cmd.AddValue("lanLinkType", "LAN link type: Csma or PointToPoint", lanLinkType);
    cmd.AddValue("csmaRate", "CSMA link data rate (e.g., 100Mbps or 10Mbps)", csmaRate);
    cmd.AddValue("csmaDelay", "CSMA link delay in ms", csmaDelay);
    cmd.AddValue("wanRate", "WAN link data rate (e.g., 5Mbps)", wanRate);
    cmd.AddValue("wanDelay", "WAN link delay (e.g., 50ms)", wanDelay);
    cmd.Parse(argc, argv);

    Time csmaTimeDelay = MilliSeconds(csmaDelay);

    // Create nodes

    // Top LAN
    NodeContainer topNodes;
    topNodes.Create(4); // t2, tr, switches will be handled via bridge
    Ptr<Node> t2 = topNodes.Get(0);
    Ptr<Node> tr = topNodes.Get(3);

    // Bottom LAN
    NodeContainer bottomNodes;
    bottomNodes.Create(4); // b2, br, switches will be handled via bridge
    Ptr<Node> b3 = bottomNodes.Get(0);
    Ptr<Node> br = bottomNodes.Get(3);

    // WAN routers
    NodeContainer wanRouters;
    wanRouters.Add(tr);
    wanRouters.Add(br);

    // Create point-to-point WAN link
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue(wanRate));
    p2p.SetChannelAttribute("Delay", StringValue(wanDelay));

    NetDeviceContainer wanDevices;
    wanDevices = p2p.Install(wanRouters);

    // Setup LANs with bridges

    InternetStackHelper stack;
    stack.Install(topNodes);
    stack.Install(bottomNodes);
    stack.Install(wanRouters);

    // Top LAN: t2 connected through multiple switches to tr
    CsmaHelper csmaTop;
    csmaTop.SetChannelAttribute("DataRate", StringValue(csmaRate));
    csmaTop.SetChannelAttribute("Delay", TimeValue(csmaTimeDelay));

    BridgeHelper bridgeTop;
    NetDeviceContainer topBridgePorts;

    // Create LAN segments and bridges
    NodeContainer topSwitches;
    topSwitches.Create(2);

    // Connect t2 -> switch1 -> switch2 -> router tr
    NetDeviceContainer devT2Switch1 = csmaTop.Install(NodeContainer(t2, topSwitches.Get(0)));
    NetDeviceContainer devSwitch1Switch2 = csmaTop.Install(NodeContainer(topSwitches.Get(0), topSwitches.Get(1)));
    NetDeviceContainer devSwitch2Tr = csmaTop.Install(NodeContainer(topSwitches.Get(1), tr));

    bridgeTop.Install(topSwitches.Get(0));
    bridgeTop.Install(topSwitches.Get(1));

    // Assign IP addresses to top LAN interfaces
    Ipv4AddressHelper ipv4Top;
    ipv4Top.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer topInterfaces;
    topInterfaces.Add(devSwitch2Tr.Get(1)); // Interface on tr

    // Bottom LAN: b3 connected through single switch to br
    CsmaHelper csmaBottom;
    csmaBottom.SetChannelAttribute("DataRate", StringValue(csmaRate));
    csmaBottom.SetChannelAttribute("Delay", TimeValue(csmaTimeDelay));

    BridgeHelper bridgeBottom;
    NetDeviceContainer bottomBridgePorts;

    NodeContainer bottomSwitch;
    bottomSwitch.Create(1);

    NetDeviceContainer devB3Switch = csmaBottom.Install(NodeContainer(b3, bottomSwitch.Get(0)));
    NetDeviceContainer devSwitchBr = csmaBottom.Install(NodeContainer(bottomSwitch.Get(0), br));

    bridgeBottom.Install(bottomSwitch.Get(0));

    // Assign IP addresses to bottom LAN interfaces
    Ipv4AddressHelper ipv4Bottom;
    ipv4Bottom.SetBase("192.168.2.0", "255.255.255.0");
    Ipv4InterfaceContainer bottomInterfaces;
    bottomInterfaces.Add(devSwitchBr.Get(1)); // Interface on br

    // Assign IP addresses to WAN interfaces
    Ipv4AddressHelper ipv4Wan;
    ipv4Wan.SetBase("10.1.1.0", "255.255.255.252");
    Ipv4InterfaceContainer wanInterfaces = ipv4Wan.Assign(wanDevices);

    // Set default routes
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Install applications
    UdpEchoServerHelper echoServer(9);

    ApplicationContainer serverAppsTop = echoServer.Install(topNodes.Get(2)); // t3 assumed as node index 2 in top LAN
    serverAppsTop.Start(Seconds(1.0));
    serverAppsTop.Stop(Seconds(10.0));

    ApplicationContainer serverAppsBottom = echoServer.Install(b3);
    serverAppsBottom.Start(Seconds(1.0));
    serverAppsBottom.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClientTop(wanInterfaces.GetAddress(1), 9);
    echoClientTop.SetAttribute("MaxPackets", UintegerValue(5));
    echoClientTop.SetAttribute("Interval", TimeValue(Seconds(1.)));
    echoClientTop.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientAppsTop = echoClientTop.Install(t2);
    clientAppsTop.Start(Seconds(2.0));
    clientAppsTop.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClientBottom(wanInterfaces.GetAddress(0), 9);
    echoClientBottom.SetAttribute("MaxPackets", UintegerValue(5));
    echoClientBottom.SetAttribute("Interval", TimeValue(Seconds(1.)));
    echoClientBottom.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientAppsBottom = echoClientBottom.Install(bottomNodes.Get(2)); // b2 assumed as node index 2 in bottom LAN
    clientAppsBottom.Start(Seconds(2.0));
    clientAppsBottom.Stop(Seconds(10.0));

    Simulator::Run();
    Simulator::Destroy();
    return 0;
}