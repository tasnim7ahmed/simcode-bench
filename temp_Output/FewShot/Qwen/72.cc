#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/bridge-helper.h"
#include "ns3/global-router-interface.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("LanWanTopology");

int main(int argc, char *argv[]) {
    std::string lanDataRate = "100Mbps";
    std::string wanDataRate = "5Mbps";
    std::string wanDelay = "2ms";

    CommandLine cmd;
    cmd.AddValue("lanRate", "LAN link data rate (e.g., 100Mbps or 10Mbps)", lanDataRate);
    cmd.AddValue("wanRate", "WAN link data rate", wanDataRate);
    cmd.AddValue("wanDelay", "WAN link delay", wanDelay);
    cmd.Parse(argc, argv);

    // Enable logging
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create nodes for top LAN: tr (router), t1 (switch), t2 (client), t3 (server)
    NodeContainer topRouter;
    topRouter.Create(1);

    NodeContainer topSwitches;
    topSwitches.Create(1); // At least one switch to connect router and client switches

    NodeContainer topClients;
    topClients.Create(1); // t2: UDP echo client
    NodeContainer topServers;
    topServers.Create(1); // t3: UDP echo server

    // Create nodes for bottom LAN: br (router), b1 (switch), b2 (server), b3 (client)
    NodeContainer bottomRouter;
    bottomRouter.Create(1);

    NodeContainer bottomSwitches;
    bottomSwitches.Create(1); // At least one switch to connect router and server switches

    NodeContainer bottomServers;
    bottomServers.Create(1); // b2: UDP echo server
    NodeContainer bottomClients;
    bottomClients.Create(1); // b3: UDP echo client

    // Combine all nodes into a container for internet stack installation
    NodeContainer allNodes;
    allNodes.Add(topRouter);
    allNodes.Add(topSwitches);
    allNodes.Add(topClients);
    allNodes.Add(topServers);
    allNodes.Add(bottomRouter);
    allNodes.Add(bottomSwitches);
    allNodes.Add(bottomServers);
    allNodes.Add(bottomClients);

    InternetStackHelper internet;
    internet.Install(allNodes);

    // Install CSMA devices for top LAN
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue(lanDataRate));
    csma.SetChannelAttribute("Delay", TimeValue(Seconds(0.0001)));

    // Connect top router to first switch
    NetDeviceContainer topRouterToSwitch;
    topRouterToSwitch = csma.Install(NodeContainer(topRouter.Get(0), topSwitches.Get(0)));

    // Connect additional switches in top LAN (simulate multiple interconnected switches)
    BridgeHelper bridgeHelper;
    for (uint32_t i = 1; i < topSwitches.GetN(); ++i) {
        NetDeviceContainer devs = csma.Install(NodeContainer(topSwitches.Get(i - 1), topSwitches.Get(i)));
        bridgeHelper.Install(topSwitches.Get(i - 1), devs);
        bridgeHelper.Install(topSwitches.Get(i), devs);
    }

    // Connect t2 (top client) to last switch in top LAN
    NetDeviceContainer topClientDevices = csma.Install(NodeContainer(topSwitches.Get(topSwitches.GetN() - 1), topClients.Get(0)));

    // Connect t3 (top server) to a single switch
    NetDeviceContainer topServerDevices = csma.Install(NodeContainer(topSwitches.Get(0), topServers.Get(0)));

    // Install bridge on switches to allow L2 forwarding
    for (uint32_t i = 0; i < topSwitches.GetN(); ++i) {
        NetDeviceContainer devs = topRouterToSwitch;
        if (i == 0) {
            devs.Add(topServerDevices.Get(0));
        }
        if (i == topSwitches.GetN() - 1) {
            devs.Add(topClientDevices.Get(0));
        }
        bridgeHelper.Install(topSwitches.Get(i), devs);
    }

    // Repeat similar setup for bottom LAN
    NetDeviceContainer bottomRouterToSwitch = csma.Install(NodeContainer(bottomRouter.Get(0), bottomSwitches.Get(0)));

    // Connect additional switches in bottom LAN
    for (uint32_t i = 1; i < bottomSwitches.GetN(); ++i) {
        NetDeviceContainer devs = csma.Install(NodeContainer(bottomSwitches.Get(i - 1), bottomSwitches.Get(i)));
        bridgeHelper.Install(bottomSwitches.Get(i - 1), devs);
        bridgeHelper.Install(bottomSwitches.Get(i), devs);
    }

    // Connect b2 (bottom server) to last switch in bottom LAN
    NetDeviceContainer bottomServerDevices = csma.Install(NodeContainer(bottomSwitches.Get(bottomSwitches.GetN() - 1), bottomServers.Get(0)));

    // Connect b3 (bottom client) to a single switch
    NetDeviceContainer bottomClientDevices = csma.Install(NodeContainer(bottomSwitches.Get(0), bottomClients.Get(0)));

    // Install bridges on bottom switches
    for (uint32_t i = 0; i < bottomSwitches.GetN(); ++i) {
        NetDeviceContainer devs = bottomRouterToSwitch;
        if (i == 0) {
            devs.Add(bottomClientDevices.Get(0));
        }
        if (i == bottomSwitches.GetN() - 1) {
            devs.Add(bottomServerDevices.Get(0));
        }
        bridgeHelper.Install(bottomSwitches.Get(i), devs);
    }

    // Configure IP addresses for LANs
    Ipv4AddressHelper address;

    // Top LAN: 192.168.1.0/24
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer topRouterInterfaces = address.Assign(topRouterToSwitch);
    Ipv4InterfaceContainer topClientInterfaces = address.Assign(topClientDevices);
    Ipv4InterfaceContainer topServerInterfaces = address.Assign(topServerDevices);

    // Bottom LAN: 192.168.2.0/24
    address.SetBase("192.168.2.0", "255.255.255.0");
    Ipv4InterfaceContainer bottomRouterInterfaces = address.Assign(bottomRouterToSwitch);
    Ipv4InterfaceContainer bottomServerInterfaces = address.Assign(bottomServerDevices);
    Ipv4InterfaceContainer bottomClientInterfaces = address.Assign(bottomClientDevices);

    // Set up WAN link between routers
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue(wanDataRate));
    p2p.SetChannelAttribute("Delay", StringValue(wanDelay));

    NetDeviceContainer wanDevices = p2p.Install(NodeContainer(topRouter.Get(0), bottomRouter.Get(0)));

    // Assign WAN IP addresses
    address.SetBase("10.0.0.0", "255.255.255.252");
    Ipv4InterfaceContainer wanInterfaces = address.Assign(wanDevices);

    // Set default routes
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Install applications
    uint16_t port = 9;

    // Top server (t3)
    UdpEchoServerHelper topServer(port);
    ApplicationContainer topServerApp = topServer.Install(topServers.Get(0));
    topServerApp.Start(Seconds(1.0));
    topServerApp.Stop(Seconds(10.0));

    // Bottom server (b2)
    UdpEchoServerHelper bottomServer(port);
    ApplicationContainer bottomServerApp = bottomServer.Install(bottomServers.Get(0));
    bottomServerApp.Start(Seconds(1.0));
    bottomServerApp.Stop(Seconds(10.0));

    // Top client (t2) -> sends to bottom server (b2)
    UdpEchoClientHelper topClient(bottomServerInterfaces.GetAddress(0), port);
    topClient.SetAttribute("MaxPackets", UintegerValue(5));
    topClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    topClient.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer topClientApp = topClient.Install(topClients.Get(0));
    topClientApp.Start(Seconds(2.0));
    topClientApp.Stop(Seconds(10.0));

    // Bottom client (b3) -> sends to top server (t3)
    UdpEchoClientHelper bottomClient(topServerInterfaces.GetAddress(0), port);
    bottomClient.SetAttribute("MaxPackets", UintegerValue(5));
    bottomClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    bottomClient.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer bottomClientApp = bottomClient.Install(bottomClients.Get(0));
    bottomClientApp.Start(Seconds(2.0));
    bottomClientApp.Stop(Seconds(10.0));

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}