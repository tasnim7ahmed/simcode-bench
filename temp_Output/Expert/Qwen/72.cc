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
    std::string csmaRate = "100Mbps";
    std::string wanRate = "5Mbps";
    std::string wanDelay = "20ms";

    CommandLine cmd;
    cmd.AddValue("csma-rate", "LAN link data rate (e.g., 100Mbps or 10Mbps)", csmaRate);
    cmd.AddValue("wan-rate", "WAN link data rate (e.g., 5Mbps)", wanRate);
    cmd.AddValue("wan-delay", "WAN link delay (e.g., 20ms)", wanDelay);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);

    // Create nodes for top LAN
    NodeContainer topNodes;
    topNodes.Create(4); // tr, t2_sw, t2_sw2, t2

    // Create nodes for bottom LAN
    NodeContainer bottomNodes;
    bottomNodes.Create(4); // br, b2_sw, b2_sw2, b2

    // Create routers and connect them via WAN
    NodeContainer wanNodes;
    wanNodes.Create(2); // tr and br

    // Install internet stack on all nodes
    InternetStackHelper stack;
    stack.InstallAll();

    // Setup CSMA helper for LANs
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", DataRateValue(DataRate(csmaRate)));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));

    // Top LAN: t2 connected through multiple switches to router tr
    NetDeviceContainer topDevices_t2_sw, topDevices_sw_sw2, topDevices_sw2_tr;
    topDevices_t2_sw = csma.Install(NodeContainer(topNodes.Get(3), topNodes.Get(2))); // t2 <-> t2_sw2
    topDevices_sw_sw2 = csma.Install(NodeContainer(topNodes.Get(2), topNodes.Get(1))); // t2_sw2 <-> t2_sw
    topDevices_sw2_tr = csma.Install(NodeContainer(topNodes.Get(1), wanNodes.Get(0))); // t2_sw <-> tr

    // Top LAN: t3 connected directly to router tr
    NetDeviceContainer topDevices_t3_tr = csma.Install(NodeContainer(topNodes.Get(0), wanNodes.Get(0))); // t3 <-> tr

    // Bottom LAN: b2 connected through multiple switches to router br
    NetDeviceContainer bottomDevices_b2_sw, bottomDevices_sw_sw2, bottomDevices_sw2_br;
    bottomDevices_b2_sw = csma.Install(NodeContainer(bottomNodes.Get(3), bottomNodes.Get(2))); // b2 <-> b2_sw2
    bottomDevices_sw_sw2 = csma.Install(NodeContainer(bottomNodes.Get(2), bottomNodes.Get(1))); // b2_sw2 <-> b2_sw
    bottomDevices_sw2_br = csma.Install(NodeContainer(bottomNodes.Get(1), wanNodes.Get(1))); // b2_sw <-> br

    // Bottom LAN: b3 connected directly to router br
    NetDeviceContainer bottomDevices_b3_br = csma.Install(NodeContainer(bottomNodes.Get(0), wanNodes.Get(1))); // b3 <-> br

    // Setup PointToPoint helper for WAN
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue(wanRate));
    p2p.SetChannelAttribute("Delay", StringValue(wanDelay));

    NetDeviceContainer wanDevices = p2p.Install(wanNodes); // tr <-> br

    // Assign IP addresses
    Ipv4AddressHelper address;

    // Top LAN subnet 192.168.1.0/24
    address.SetBase("192.168.1.0", "255.255.255.0");
    address.Assign(topDevices_t3_tr); // assign to t3 and tr
    address.NewNetwork();
    address.Assign(topDevices_sw2_tr); // assign to tr and switch

    // Bottom LAN subnet 192.168.2.0/24
    address.SetBase("192.168.2.0", "255.255.255.0");
    address.Assign(bottomDevices_b3_br); // assign to b3 and br
    address.NewNetwork();
    address.Assign(bottomDevices_sw2_br); // assign to br and switch

    // WAN subnet 10.1.1.0/24
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer wanInterfaces = address.Assign(wanDevices);

    // Set up global routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Enable global router interface support
    GlobalRouterInterface::Enable(wanNodes.Get(0)->GetObject<GlobalRouter>());
    GlobalRouterInterface::Enable(wanNodes.Get(1)->GetObject<GlobalRouter>());

    // Install UDP Echo Server on t3 (Top LAN)
    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(topNodes.Get(0)); // t3
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // Install UDP Echo Client on b3 (Bottom LAN) to ping t3
    UdpEchoClientHelper echoClient(Ipv4Address("192.168.1.1"), 9); // assuming t3 is .1
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps = echoClient.Install(bottomNodes.Get(0)); // b3
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Install UDP Echo Server on b2 (Bottom LAN)
    UdpEchoServerHelper echoServerB2(9);
    ApplicationContainer serverB2Apps = echoServerB2.Install(bottomNodes.Get(3)); // b2
    serverB2Apps.Start(Seconds(1.0));
    serverB2Apps.Stop(Seconds(10.0));

    // Install UDP Echo Client on t2 (Top LAN) to ping b2
    UdpEchoClientHelper echoClientT2(Ipv4Address("192.168.2.1"), 9); // assuming b3 is .1
    echoClientT2.SetAttribute("MaxPackets", UintegerValue(5));
    echoClientT2.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClientT2.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientT2Apps = echoClientT2.Install(topNodes.Get(3)); // t2
    clientT2Apps.Start(Seconds(2.0));
    clientT2Apps.Stop(Seconds(10.0));

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}