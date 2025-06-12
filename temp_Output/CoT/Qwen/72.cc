#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("LanWanUdpEchoSimulation");

int main(int argc, char *argv[]) {
    std::string lanType = "100Mbps";
    std::string wanRate = "5Mbps";
    std::string wanDelay = "10ms";

    CommandLine cmd;
    cmd.AddValue("lanType", "LAN link type: 100Mbps or 10Mbps", lanType);
    cmd.AddValue("wanRate", "WAN link data rate", wanRate);
    cmd.AddValue("wanDelay", "WAN link delay", wanDelay);
    cmd.Parse(argc, argv);

    bool useHighSpeedLan = (lanType == "100Mbps");

    NodeContainer topLanNodes;
    topLanNodes.Create(3); // t2, tr, and a switch node

    NodeContainer bottomLanNodes;
    bottomLanNodes.Create(3); // b2, br, and a switch node

    NodeContainer wanRouters;
    wanRouters.Add(topLanNodes.Get(1)); // tr
    wanRouters.Add(bottomLanNodes.Get(1)); // br

    CsmaHelper csmaLan;
    if (useHighSpeedLan) {
        csmaLan.SetChannelAttribute("DataRate", DataRateValue(DataRate("100Mbps")));
    } else {
        csmaLan.SetChannelAttribute("DataRate", DataRateValue(DataRate("10Mbps")));
    }
    csmaLan.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));

    PointToPointHelper p2pWan;
    p2pWan.SetDeviceAttribute("DataRate", StringValue(wanRate));
    p2pWan.SetChannelAttribute("Delay", StringValue(wanDelay));

    NetDeviceContainer topLanDevices;
    NetDeviceContainer bottomLanDevices;
    NetDeviceContainer wanDevices;

    // Top LAN setup
    NodeContainer topSwitches;
    topSwitches.Create(2);
    NodeContainer t2 = topLanNodes.Get(0);
    NodeContainer tr = topLanNodes.Get(1);

    NodeContainer t2_sw0 = NodeContainer(t2, topSwitches.Get(0));
    NodeContainer sw0_sw1 = NodeContainer(topSwitches.Get(0), topSwitches.Get(1));
    NodeContainer sw1_tr = NodeContainer(topSwitches.Get(1), tr);

    topLanDevices.Add(csmaLan.Install(t2_sw0));
    topLanDevices.Add(csmaLan.Install(sw0_sw1));
    topLanDevices.Add(csmaLan.Install(sw1_tr));

    // Add t3 (UDP echo server) to top LAN through a single switch
    Ptr<Node> t3 = CreateObject<Node>();
    topLanNodes.Add(t3);
    NodeContainer sw1_t3 = NodeContainer(topSwitches.Get(1), t3);
    topLanDevices.Add(csmaLan.Install(sw1_t3));

    // Bottom LAN setup
    NodeContainer bottomSwitches;
    bottomSwitches.Create(2);
    NodeContainer b2 = bottomLanNodes.Get(0);
    NodeContainer br = bottomLanNodes.Get(1);

    NodeContainer b2_sw0 = NodeContainer(b2, bottomSwitches.Get(0));
    NodeContainer sw0_sw1_bottom = NodeContainer(bottomSwitches.Get(0), bottomSwitches.Get(1));
    NodeContainer sw1_br = NodeContainer(bottomSwitches.Get(1), br);

    bottomLanDevices.Add(csmaLan.Install(b2_sw0));
    bottomLanDevices.Add(csmaLan.Install(sw0_sw1_bottom));
    bottomLanDevices.Add(csmaLan.Install(sw1_br));

    // Add b3 (UDP echo client) to bottom LAN through a single switch
    Ptr<Node> b3 = CreateObject<Node>();
    bottomLanNodes.Add(b3);
    NodeContainer sw1_b3 = NodeContainer(bottomSwitches.Get(1), b3);
    bottomLanDevices.Add(csmaLan.Install(sw1_b3));

    // WAN link between routers
    wanDevices = p2pWan.Install(wanRouters);

    InternetStackHelper internet;
    internet.InstallAll();

    Ipv4AddressHelper address;

    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer topInterfaces;
    for (NetDeviceContainer::Iterator iter = topLanDevices.Begin(); iter != topLanDevices.End(); ++iter) {
        topInterfaces.Add(address.Assign(*iter));
    }

    address.SetBase("192.168.2.0", "255.255.255.0");
    Ipv4InterfaceContainer bottomInterfaces;
    for (NetDeviceContainer::Iterator iter = bottomLanDevices.Begin(); iter != bottomLanDevices.End(); ++iter) {
        bottomInterfaces.Add(address.Assign(*iter));
    }

    address.SetBase("10.1.1.0", "255.255.255.252");
    Ipv4InterfaceContainer wanInterfaces = address.Assign(wanDevices);

    // Set up routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // UDP Echo Server on t3 (top LAN)
    UdpEchoServerHelper echoServerTop(9);
    ApplicationContainer serverAppsTop = echoServerTop.Install(t3);
    serverAppsTop.Start(Seconds(1.0));
    serverAppsTop.Stop(Seconds(10.0));

    // UDP Echo Client on b3 (bottom LAN), sending to t3
    UdpEchoClientHelper echoClientBottom(topInterfaces.GetAddress(3), 9);
    echoClientBottom.SetAttribute("MaxPackets", UintegerValue(5));
    echoClientBottom.SetAttribute("Interval", TimeValue(Seconds(1.)));
    echoClientBottom.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientAppsBottom = echoClientBottom.Install(b3);
    clientAppsBottom.Start(Seconds(2.0));
    clientAppsBottom.Stop(Seconds(10.0));

    // UDP Echo Server on b2 (bottom LAN)
    UdpEchoServerHelper echoServerBottom(9);
    ApplicationContainer serverAppsBottom = echoServerBottom.Install(b2);
    serverAppsBottom.Start(Seconds(1.0));
    serverAppsBottom.Stop(Seconds(10.0));

    // UDP Echo Client on t2 (top LAN), sending to b2
    UdpEchoClientHelper echoClientTop(bottomInterfaces.GetAddress(0), 9);
    echoClientTop.SetAttribute("MaxPackets", UintegerValue(5));
    echoClientTop.SetAttribute("Interval", TimeValue(Seconds(1.)));
    echoClientTop.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientAppsTop = echoClientTop.Install(t2);
    clientAppsTop.Start(Seconds(3.0));
    clientAppsTop.Stop(Seconds(10.0));

    Simulator::Run();
    Simulator::Destroy();
    return 0;
}