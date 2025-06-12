#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    std::string csmaRate = "100Mbps";
    bool enablePcap = true;

    CommandLine cmd(__FILE__);
    cmd.AddValue("csmaRate", "CSMA link data rate (e.g., 100Mbps or 10Mbps)", csmaRate);
    cmd.AddValue("pcap", "Enable PCAP tracing", enablePcap);
    cmd.Parse(argc, argv);

    // Create nodes for top LAN
    NodeContainer topSwitches;
    topSwitches.Create(4); // ts1, ts2, ts3, ts4

    NodeContainer topClients;
    topClients.Create(2); // t2, t3

    // Create nodes for bottom LAN
    NodeContainer bottomSwitches;
    bottomSwitches.Create(5); // bs1 to bs5

    NodeContainer bottomClients;
    bottomClients.Create(2); // b2, b3

    // Create routers
    NodeContainer routers;
    routers.Create(2); // tr, br

    // Combine all nodes into a single container for IP stack installation
    NodeContainer allNodes;
    allNodes.Add(topSwitches);
    allNodes.Add(topClients);
    allNodes.Add(bottomSwitches);
    allNodes.Add(bottomClients);
    allNodes.Add(routers);

    InternetStackHelper internet;
    internet.Install(allNodes);

    // Top LAN CSMA connections: fully connected switches
    CsmaHelper csmaTop;
    csmaTop.SetChannelAttribute("DataRate", StringValue(csmaRate));
    csmaTop.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));

    NetDeviceContainer topDevices;
    for (uint32_t i = 0; i < topSwitches.GetN(); ++i) {
        for (uint32_t j = i + 1; j < topSwitches.GetN(); ++j) {
            NodeContainer pair(topSwitches.Get(i), topSwitches.Get(j));
            topDevices.Add(csmaTop.Install(pair));
        }
    }

    // Connect t2 and t3 to top LAN
    topDevices.Add(csmaTop.Install(NodeContainer(topSwitches.Get(0), topClients.Get(0))));
    topDevices.Add(csmaTop.Install(NodeContainer(topSwitches.Get(3), topClients.Get(1))));

    // Bottom LAN CSMA connections: linear switch topology
    CsmaHelper csmaBottom;
    csmaBottom.SetChannelAttribute("DataRate", StringValue(csmaRate));
    csmaBottom.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));

    NetDeviceContainer bottomDevices;
    for (uint32_t i = 0; i < bottomSwitches.GetN() - 1; ++i) {
        NodeContainer pair(bottomSwitches.Get(i), bottomSwitches.Get(i+1));
        bottomDevices.Add(csmaBottom.Install(pair));
    }

    // Connect b2 and b3 to bottom LAN
    bottomDevices.Add(csmaBottom.Install(NodeContainer(bottomSwitches.Get(0), bottomClients.Get(0))));
    bottomDevices.Add(csmaBottom.Install(NodeContainer(bottomSwitches.Get(2), bottomClients.Get(1))));

    // WAN connection between routers
    PointToPointHelper p2pWan;
    p2pWan.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2pWan.SetChannelAttribute("Delay", StringValue("50ms"));

    NetDeviceContainer wanDevices = p2pWan.Install(NodeContainer(routers.Get(0), routers.Get(1)));

    // Connect routers to LANs
    NetDeviceContainer routerTopDevices = csmaTop.Install(NodeContainer(routers.Get(0), topSwitches.Get(1)));
    NetDeviceContainer routerBottomDevices = csmaBottom.Install(NodeContainer(routers.Get(1), bottomSwitches.Get(4)));

    // Assign IP addresses
    Ipv4AddressHelper ip;

    // Top LAN subnet
    ip.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer topInterfaces;
    for (NetDeviceContainer::Iterator it = topDevices.Begin(); it != topDevices.End(); ++it) {
        topInterfaces.Add(ip.Assign(*it));
    }

    // Bottom LAN subnet
    ip.SetBase("192.168.2.0", "255.255.255.0");
    Ipv4InterfaceContainer bottomInterfaces;
    for (NetDeviceContainer::Iterator it = bottomDevices.Begin(); it != bottomDevices.End(); ++it) {
        bottomInterfaces.Add(ip.Assign(*it));
    }

    // WAN subnet
    ip.SetBase("76.1.1.0", "255.255.255.252");
    Ipv4InterfaceContainer wanInterfaces = ip.Assign(wanDevices);

    // Router interfaces on LAN subnets
    Ipv4InterfaceContainer routerTopInterface = ip.Assign(routerTopDevices);
    Ipv4InterfaceContainer routerBottomInterface = ip.Assign(routerBottomDevices);

    // Set up default routes
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // UDP Echo Server applications
    uint16_t echoPort = 9;

    UdpEchoServerHelper echoServer(echoPort);
    ApplicationContainer serverApps;

    serverApps.Add(echoServer.Install(bottomClients.Get(0))); // b2 as server
    serverApps.Add(echoServer.Install(bottomClients.Get(1))); // b3 as server

    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // UDP Echo Client applications
    ApplicationContainer clientApps;

    UdpEchoClientHelper echoClient1(routerBottomInterface.GetAddress(1), echoPort);
    echoClient1.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient1.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient1.SetAttribute("PacketSize", UintegerValue(1024));
    clientApps.Add(echoClient1.Install(topClients.Get(0))); // t2 -> b2 via multiple switches

    UdpEchoClientHelper echoClient2(routerBottomInterface.GetAddress(1), echoPort);
    echoClient2.SetAttribute("RemoteAddress", Ipv4AddressValue(bottomInterfaces.GetAddress(3)));
    echoClient2.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient2.SetAttribute("Interval", TimeValue(Seconds(2.0)));
    echoClient2.SetAttribute("PacketSize", UintegerValue(512));
    clientApps.Add(echoClient2.Install(topClients.Get(1))); // t3 -> b3 through single switch

    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Enable logging
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Enable PCAP traces
    if (enablePcap) {
        csmaTop.EnablePcapAll("top_lan");
        csmaBottom.EnablePcapAll("bottom_lan");
        p2pWan.EnablePcapAll("wan_link");
    }

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}