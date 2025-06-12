#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("LanWanLanSimulation");

int main(int argc, char *argv[]) {
    std::string csmaRate = "100Mbps";

    CommandLine cmd(__FILE__);
    cmd.AddValue("csmaRate", "CSMA link rate (e.g., 10Mbps or 100Mbps)", csmaRate);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create nodes for top LAN: ts1-ts4 are switches, t2-t3 are endpoints
    NodeContainer topSwitches;
    topSwitches.Create(4);
    NodeContainer topEnds;
    topEnds.Create(2);
    NodeContainer topRouter;
    topRouter.Create(1);

    // Create nodes for bottom LAN: bs1-bs5 are switches, b2-b3 are endpoints
    NodeContainer bottomSwitches;
    bottomSwitches.Create(5);
    NodeContainer bottomEnds;
    bottomEnds.Create(2);
    NodeContainer bottomRouter;
    bottomRouter.Create(1);

    // Create WAN link between routers
    PointToPointHelper wanP2P;
    wanP2P.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    wanP2P.SetChannelAttribute("Delay", StringValue("50ms"));

    NetDeviceContainer wanDevices = wanP2P.Install(topRouter.Get(0), bottomRouter.Get(0));

    // CSMA Helper for LANs
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue(csmaRate));
    csma.SetChannelAttribute("Delay", StringValue("2ms"));

    // Install internet stack on all non-switch nodes
    InternetStackHelper stack;
    for (uint32_t i = 0; i < topEnds.GetN(); ++i) {
        stack.Install(topEnds.Get(i));
    }
    for (uint32_t i = 0; i < bottomEnds.GetN(); ++i) {
        stack.Install(bottomEnds.Get(i));
    }
    stack.Install(topRouter.Get(0));
    stack.Install(bottomRouter.Get(0));

    // Containers for IP assignment
    Ipv4AddressHelper address;

    // Assign IP to WAN
    address.SetBase("76.1.1.0", "255.255.255.252");
    Ipv4InterfaceContainer wanInterfaces = address.Assign(wanDevices);

    // Top LAN network: connect switches in a chain, then connect endpoints and router
    std::vector<NetDeviceContainer> topSwitchLinks;
    std::vector<Ipv4InterfaceContainer> topSwitchInterfaces;

    for (uint32_t i = 0; i < topSwitches.GetN() - 1; ++i) {
        NetDeviceContainer devs = csma.Install(NodeContainer(topSwitches.Get(i), topSwitches.Get(i+1)));
        topSwitchLinks.push_back(devs);
    }

    // Connect endpoint t2 to ts1
    NetDeviceContainer t2Dev = csma.Install(NodeContainer(topSwitches.Get(0), topEnds.Get(0)));
    // Connect endpoint t3 to ts4
    NetDeviceContainer t3Dev = csma.Install(NodeContainer(topSwitches.Get(3), topEnds.Get(1)));
    // Connect ts1 to top router
    NetDeviceContainer trDev = csma.Install(NodeContainer(topSwitches.Get(0), topRouter.Get(0)));

    // Assign IPs to top LAN interfaces
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer topLanInterfaces;
    topLanInterfaces.Add(address.Assign(t2Dev.Get(1))); // t2
    topLanInterfaces.Add(address.Assign(t3Dev.Get(1))); // t3
    topLanInterfaces.Add(address.Assign(trDev.Get(1)));  // top router interface

    // Bottom LAN network: connect switches in a chain, then connect endpoints and router
    std::vector<NetDeviceContainer> bottomSwitchLinks;
    std::vector<Ipv4InterfaceContainer> bottomSwitchInterfaces;

    for (uint32_t i = 0; i < bottomSwitches.GetN() - 1; ++i) {
        NetDeviceContainer devs = csma.Install(NodeContainer(bottomSwitches.Get(i), bottomSwitches.Get(i+1)));
        bottomSwitchLinks.push_back(devs);
    }

    // Connect endpoint b2 to bs1
    NetDeviceContainer b2Dev = csma.Install(NodeContainer(bottomSwitches.Get(0), bottomEnds.Get(0)));
    // Connect endpoint b3 to bs3
    NetDeviceContainer b3Dev = csma.Install(NodeContainer(bottomSwitches.Get(2), bottomEnds.Get(1)));
    // Connect bs1 to bottom router
    NetDeviceContainer brDev = csma.Install(NodeContainer(bottomSwitches.Get(0), bottomRouter.Get(0)));

    // Assign IPs to bottom LAN interfaces
    address.SetBase("192.168.2.0", "255.255.255.0");
    Ipv4InterfaceContainer bottomLanInterfaces;
    bottomLanInterfaces.Add(address.Assign(b2Dev.Get(1))); // b2
    bottomLanInterfaces.Add(address.Assign(b3Dev.Get(1))); // b3
    bottomLanInterfaces.Add(address.Assign(brDev.Get(1)));  // bottom router interface

    // Set up global routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // UDP Echo Applications: b2 and b3 as servers, t2 and t3 as clients
    uint16_t port = 9;

    // Server on b2
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApps = echoServer.Install(bottomEnds.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // Client on t2 connecting to b2
    UdpEchoClientHelper echoClient(bottomLanInterfaces.GetAddress(0), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps = echoClient.Install(topEnds.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Server on b3
    port = 10;
    UdpEchoServerHelper echoServer2(port);
    ApplicationContainer serverApps2 = echoServer2.Install(bottomEnds.Get(1));
    serverApps2.Start(Seconds(1.0));
    serverApps2.Stop(Seconds(10.0));

    // Client on t3 connecting to b3
    UdpEchoClientHelper echoClient2(bottomLanInterfaces.GetAddress(1), port);
    echoClient2.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient2.SetAttribute("Interval", TimeValue(Seconds(1.)));
    echoClient2.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps2 = echoClient2.Install(topEnds.Get(1));
    clientApps2.Start(Seconds(2.0));
    clientApps2.Stop(Seconds(10.0));

    // Enable PCAP traces at key points
    wanP2P.EnablePcapAll("wan-link");
    csma.EnablePcap("top-router", trDev, true, true);
    csma.EnablePcap("bottom-router", brDev, true, true);
    csma.EnablePcap("t2-endpoint", t2Dev, true, true);
    csma.EnablePcap("b3-endpoint", b3Dev, true, true);

    Simulator::Run();
    Simulator::Destroy();
    return 0;
}