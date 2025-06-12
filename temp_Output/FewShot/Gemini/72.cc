#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/csma-module.h"
#include "ns3/bridge-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    bool verbose = false;
    std::string lanLinkDataRate = "100Mbps";
    std::string wanLinkDataRate = "10Mbps";
    std::string wanLinkDelay = "2ms";

    CommandLine cmd;
    cmd.AddValue("verbose", "Tell applications to log if true", verbose);
    cmd.AddValue("lanLinkDataRate", "LAN link data rate", lanLinkDataRate);
    cmd.AddValue("wanLinkDataRate", "WAN link data rate", wanLinkDataRate);
    cmd.AddValue("wanLinkDelay", "WAN link delay", wanLinkDelay);
    cmd.Parse(argc, argv);

    if (verbose) {
        LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
        LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
    }

    // Create nodes
    NodeContainer topNodes, bottomNodes, routers;
    topNodes.Create(3);   // t1, t2, t3
    bottomNodes.Create(3); // b1, b2, b3
    routers.Create(2);    // tr, br

    // Name the nodes
    Names::Add("t1", topNodes.Get(0));
    Names::Add("t2", topNodes.Get(1));
    Names::Add("t3", topNodes.Get(2));
    Names::Add("b1", bottomNodes.Get(0));
    Names::Add("b2", bottomNodes.Get(1));
    Names::Add("b3", bottomNodes.Get(2));
    Names::Add("tr", routers.Get(0));
    Names::Add("br", routers.Get(1));

    // Create switches
    NodeContainer topSwitches, bottomSwitches;
    topSwitches.Create(2);
    bottomSwitches.Create(2);

    // TOP LAN
    CsmaHelper topCsma;
    topCsma.SetChannelAttribute("DataRate", StringValue(lanLinkDataRate));
    topCsma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));

    NetDeviceContainer topDevices;
    NetDeviceContainer topSwitchDevices1, topSwitchDevices2;

    // Connect t2 to switch1
    NodeContainer t2_s1;
    t2_s1.Add(topNodes.Get(1));
    t2_s1.Add(topSwitches.Get(0));
    topSwitchDevices1 = topCsma.Install(t2_s1);
    topDevices.Add(topSwitchDevices1);

    // Connect switch1 to switch2
    NodeContainer s1_s2;
    s1_s2.Add(topSwitches.Get(0));
    s1_s2.Add(topSwitches.Get(1));
    topSwitchDevices2 = topCsma.Install(s1_s2);
    topDevices.Add(topSwitchDevices2);

    // Connect switch2 to tr
    NodeContainer s2_tr;
    s2_tr.Add(topSwitches.Get(1));
    s2_tr.Add(routers.Get(0));
    NetDeviceContainer topSwitchDevices3 = topCsma.Install(s2_tr);
    topDevices.Add(topSwitchDevices3);

    // Connect t3 to tr through switch
    NodeContainer t3_s2_tr;
    t3_s2_tr.Add(topNodes.Get(2));
    t3_s2_tr.Add(routers.Get(0));
    NetDeviceContainer topSwitchDevices4 = topCsma.Install(t3_s2_tr);
    topDevices.Add(topSwitchDevices4);

    // BOTTOM LAN
    CsmaHelper bottomCsma;
    bottomCsma.SetChannelAttribute("DataRate", StringValue(lanLinkDataRate));
    bottomCsma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));

    NetDeviceContainer bottomDevices;
    NetDeviceContainer bottomSwitchDevices1, bottomSwitchDevices2;

    // Connect b2 to br through switches
     NodeContainer b2_s1_s2_br;
    b2_s1_s2_br.Add(bottomNodes.Get(1));
    b2_s1_s2_br.Add(bottomSwitches.Get(0));
    bottomSwitchDevices1 = bottomCsma.Install(b2_s1_s2_br);
    bottomDevices.Add(bottomSwitchDevices1);
    NodeContainer b2_s2_br;
    b2_s2_br.Add(bottomSwitches.Get(0));
    b2_s2_br.Add(bottomSwitches.Get(1));
    bottomSwitchDevices2 = bottomCsma.Install(b2_s2_br);
    bottomDevices.Add(bottomSwitchDevices2);
    NodeContainer b2_br;
    b2_br.Add(bottomSwitches.Get(1));
    b2_br.Add(routers.Get(1));
    NetDeviceContainer bottomSwitchDevices3 = bottomCsma.Install(b2_br);
    bottomDevices.Add(bottomSwitchDevices3);

    // Connect b3 to br
    NodeContainer b3_br;
    b3_br.Add(bottomNodes.Get(2));
    b3_br.Add(routers.Get(1));
    NetDeviceContainer bottomSwitchDevices4 = bottomCsma.Install(b3_br);
    bottomDevices.Add(bottomSwitchDevices4);

    // Install Internet stack
    InternetStackHelper stack;
    stack.InstallAll();

    // Assign IP addresses
    Ipv4AddressHelper topAddress, bottomAddress, wanAddress;
    topAddress.SetBase("192.168.1.0", "255.255.255.0");
    bottomAddress.SetBase("192.168.2.0", "255.255.255.0");
    wanAddress.SetBase("10.1.1.0", "255.255.255.0");

    Ipv4InterfaceContainer topInterfaces = topAddress.Assign(topDevices);
    Ipv4InterfaceContainer bottomInterfaces = bottomAddress.Assign(bottomDevices);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // WAN link
    PointToPointHelper wanPointToPoint;
    wanPointToPoint.SetDeviceAttribute("DataRate", StringValue(wanLinkDataRate));
    wanPointToPoint.SetChannelAttribute("Delay", StringValue(wanLinkDelay));

    NetDeviceContainer wanDevices = wanPointToPoint.Install(routers);
    Ipv4InterfaceContainer wanInterfaces = wanAddress.Assign(wanDevices);
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Bridge Setup
    BridgeHelper bridge;
    NetDeviceContainer bridgeDevices;
    bridgeDevices.Add(topCsma.Install(topNodes.Get(0)));
    bridgeDevices.Add(bottomCsma.Install(bottomNodes.Get(0)));
    bridge.Install(routers.Get(0), bridgeDevices);
    bridge.Install(routers.Get(1), bridgeDevices);

    // Applications
    uint16_t port = 9;

    // Top LAN
    UdpEchoServerHelper topServer(port);
    ApplicationContainer topServerApp = topServer.Install(topNodes.Get(2)); // t3
    topServerApp.Start(Seconds(1.0));
    topServerApp.Stop(Seconds(10.0));

    UdpEchoClientHelper topClient(topInterfaces.GetAddress(2), port); //t3
    topClient.SetAttribute("MaxPackets", UintegerValue(5));
    topClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    topClient.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer topClientApp = topClient.Install(topNodes.Get(1)); // t2
    topClientApp.Start(Seconds(2.0));
    topClientApp.Stop(Seconds(10.0));

    // Bottom LAN
    UdpEchoServerHelper bottomServer(port);
    ApplicationContainer bottomServerApp = bottomServer.Install(bottomNodes.Get(1)); // b2
    bottomServerApp.Start(Seconds(1.0));
    bottomServerApp.Stop(Seconds(10.0));

    UdpEchoClientHelper bottomClient(bottomInterfaces.GetAddress(1), port); //b2
    bottomClient.SetAttribute("MaxPackets", UintegerValue(5));
    bottomClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    bottomClient.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer bottomClientApp = bottomClient.Install(bottomNodes.Get(2)); // b3
    bottomClientApp.Start(Seconds(2.0));
    bottomClientApp.Stop(Seconds(10.0));

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}