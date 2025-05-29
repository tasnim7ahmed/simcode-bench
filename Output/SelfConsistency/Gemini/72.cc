#include <iostream>
#include <fstream>
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/bridge-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("MultiSwitchWan");

int main(int argc, char *argv[]) {
    bool verbose = false;
    std::string wanDataRate = "10Mbps";
    std::string wanDelay = "2ms";
    std::string lanDataRate100 = "100Mbps";
    std::string lanDataRate10 = "10Mbps";

    CommandLine cmd;
    cmd.AddValue("verbose", "Tell application to log if true", verbose);
    cmd.AddValue("wanDataRate", "Wan link data rate", wanDataRate);
    cmd.AddValue("wanDelay", "Wan link delay", wanDelay);
    cmd.AddValue("lanDataRate100", "LAN link data rate (100Mbps)", lanDataRate100);
    cmd.AddValue("lanDataRate10", "LAN link data rate (10Mbps)", lanDataRate10);
    cmd.Parse(argc, argv);

    if (verbose) {
        LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
        LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
    }

    // Create nodes
    NodeContainer topNodes;
    topNodes.Create(4); // t0, t1, t2 (client), t3 (server)

    NodeContainer bottomNodes;
    bottomNodes.Create(4); // b0, b1, b2 (server), b3 (client)

    NodeContainer routers;
    routers.Create(2); // tr, br

    // Create switches
    NodeContainer topSwitches;
    topSwitches.Create(2); // ts0, ts1

    NodeContainer bottomSwitches;
    bottomSwitches.Create(2); // bs0, bs1

    // Create links - TOP LAN
    CsmaHelper topCsma100;
    topCsma100.SetChannelAttribute("DataRate", StringValue(lanDataRate100));
    topCsma100.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));

    CsmaHelper topCsma10;
    topCsma10.SetChannelAttribute("DataRate", StringValue(lanDataRate10));
    topCsma10.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));

    NetDeviceContainer topDev0 = topCsma100.Install(NodeContainer(topNodes.Get(0), topSwitches.Get(0)));
    NetDeviceContainer topDev1 = topCsma100.Install(NodeContainer(topSwitches.Get(0), topSwitches.Get(1)));
    NetDeviceContainer topDev2 = topCsma100.Install(NodeContainer(topSwitches.Get(1), topNodes.Get(2)));
    NetDeviceContainer topDev3 = topCsma10.Install(NodeContainer(topNodes.Get(3), topSwitches.Get(0)));
    NetDeviceContainer topDevRouter = topCsma100.Install(NodeContainer(routers.Get(0), topSwitches.Get(1)));

    // Create links - BOTTOM LAN
    CsmaHelper bottomCsma100;
    bottomCsma100.SetChannelAttribute("DataRate", StringValue(lanDataRate100));
    bottomCsma100.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));

    CsmaHelper bottomCsma10;
    bottomCsma10.SetChannelAttribute("DataRate", StringValue(lanDataRate10));
    bottomCsma10.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));

    NetDeviceContainer bottomDev0 = bottomCsma100.Install(NodeContainer(bottomNodes.Get(0), bottomSwitches.Get(0)));
    NetDeviceContainer bottomDev1 = bottomCsma100.Install(NodeContainer(bottomSwitches.Get(0), bottomSwitches.Get(1)));
    NetDeviceContainer bottomDev2 = bottomCsma100.Install(NodeContainer(bottomSwitches.Get(1), bottomNodes.Get(2)));
    NetDeviceContainer bottomDev3 = bottomCsma10.Install(NodeContainer(bottomNodes.Get(3), bottomSwitches.Get(0)));
    NetDeviceContainer bottomDevRouter = bottomCsma100.Install(NodeContainer(routers.Get(1), bottomSwitches.Get(1)));

    // Create WAN link
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue(wanDataRate));
    pointToPoint.SetChannelAttribute("Delay", StringValue(wanDelay));

    NetDeviceContainer wanDevices = pointToPoint.Install(routers);

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(topNodes);
    stack.Install(bottomNodes);
    stack.Install(routers);
    stack.Install(topSwitches);
    stack.Install(bottomSwitches);

    // Assign addresses
    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer topInterfaces0 = address.Assign(topDev0);
    Ipv4InterfaceContainer topInterfaces1 = address.Assign(topDev1);
    Ipv4InterfaceContainer topInterfaces2 = address.Assign(topDev2);
    Ipv4InterfaceContainer topInterfaces3 = address.Assign(topDev3);
    Ipv4InterfaceContainer topInterfacesRouter = address.Assign(topDevRouter);

    address.SetBase("192.168.2.0", "255.255.255.0");
    Ipv4InterfaceContainer bottomInterfaces0 = address.Assign(bottomDev0);
    Ipv4InterfaceContainer bottomInterfaces1 = address.Assign(bottomDev1);
    Ipv4InterfaceContainer bottomInterfaces2 = address.Assign(bottomDev2);
    Ipv4InterfaceContainer bottomInterfaces3 = address.Assign(bottomDev3);
    Ipv4InterfaceContainer bottomInterfacesRouter = address.Assign(bottomDevRouter);

    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer wanInterfaces = address.Assign(wanDevices);

    // Configure routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Configure UDP echo applications
    UdpEchoServerHelper echoServerTop(9);
    ApplicationContainer serverAppsTop = echoServerTop.Install(topNodes.Get(3));
    serverAppsTop.Start(Seconds(1.0));
    serverAppsTop.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClientTop(topInterfaces3.GetAddress(0), 9);
    echoClientTop.SetAttribute("MaxPackets", UintegerValue(1));
    echoClientTop.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClientTop.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientAppsTop = echoClientTop.Install(topNodes.Get(2));
    clientAppsTop.Start(Seconds(2.0));
    clientAppsTop.Stop(Seconds(10.0));

    UdpEchoServerHelper echoServerBottom(9);
    ApplicationContainer serverAppsBottom = echoServerBottom.Install(bottomNodes.Get(2));
    serverAppsBottom.Start(Seconds(1.0));
    serverAppsBottom.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClientBottom(bottomInterfaces2.GetAddress(0), 9);
    echoClientBottom.SetAttribute("MaxPackets", UintegerValue(1));
    echoClientBottom.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClientBottom.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientAppsBottom = echoClientBottom.Install(bottomNodes.Get(3));
    clientAppsBottom.Start(Seconds(2.0));
    clientAppsBottom.Stop(Seconds(10.0));

    // Enable PCAP tracing
    //pointToPoint.EnablePcapAll("multi-switch-wan");

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}