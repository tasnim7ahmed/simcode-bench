#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/bridge-module.h"
#include "ns3/global-route-manager.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("MultiLanWanSimulation");

int main(int argc, char* argv[]) {
    bool tracing = true;
    std::string lanLinkSpeed = "100Mbps";

    CommandLine cmd;
    cmd.AddValue("tracing", "Enable or disable PCAP tracing", tracing);
    cmd.AddValue("lanLinkSpeed", "LAN link speed (100Mbps or 10Mbps)", lanLinkSpeed);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);

    //
    // Create nodes
    //
    NodeContainer topRouters;
    topRouters.Create(1);

    NodeContainer bottomRouters;
    bottomRouters.Create(1);

    NodeContainer topSwitches;
    topSwitches.Create(4);

    NodeContainer bottomSwitches;
    bottomSwitches.Create(5);

    NodeContainer topEndpoints;
    topEndpoints.Create(2);

    NodeContainer bottomEndpoints;
    bottomEndpoints.Create(2);

    //
    // Create channels
    //

    // WAN link
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("50ms"));
    NetDeviceContainer wanDevices = pointToPoint.Install(topRouters.Get(0), bottomRouters.Get(0));

    // Top LAN
    CsmaHelper topCsma;
    topCsma.SetChannelAttribute("DataRate", StringValue(lanLinkSpeed));
    topCsma.SetChannelAttribute("Delay", StringValue("2us"));

    NetDeviceContainer topSwitchDevices;
    NetDeviceContainer topEndpointDevices;

    for (uint32_t i = 0; i < topSwitches.GetN(); ++i) {
        NetDeviceContainer switchDev = topCsma.Install(NodeContainer(topRouters.Get(0), topSwitches.Get(i)));
        topSwitchDevices.Add(switchDev.Get(1));
    }
    for (uint32_t i = 0; i < topEndpoints.GetN(); ++i) {
        NetDeviceContainer endpointDev = topCsma.Install(NodeContainer(topSwitches.Get(i), topEndpoints.Get(i)));
        topEndpointDevices.Add(endpointDev.Get(1));
    }

    NetDeviceContainer topLANRouterDev = topCsma.Install(NodeContainer(topRouters.Get(0), topSwitches.Get(3)));
    topSwitchDevices.Add(topLANRouterDev.Get(1));

    // Bottom LAN
    CsmaHelper bottomCsma;
    bottomCsma.SetChannelAttribute("DataRate", StringValue(lanLinkSpeed));
    bottomCsma.SetChannelAttribute("Delay", StringValue("2us"));

    NetDeviceContainer bottomSwitchDevices;
    NetDeviceContainer bottomEndpointDevices;

    for (uint32_t i = 0; i < bottomSwitches.GetN(); ++i) {
        NetDeviceContainer switchDev = bottomCsma.Install(NodeContainer(bottomRouters.Get(0), bottomSwitches.Get(i)));
        bottomSwitchDevices.Add(switchDev.Get(1));
    }

    for (uint32_t i = 0; i < bottomEndpoints.GetN(); ++i) {
        NetDeviceContainer endpointDev = bottomCsma.Install(NodeContainer(bottomSwitches.Get(i), bottomEndpoints.Get(i)));
        bottomEndpointDevices.Add(endpointDev.Get(1));
    }
    NetDeviceContainer bottomLANRouterDev = bottomCsma.Install(NodeContainer(bottomRouters.Get(0), bottomSwitches.Get(4)));
    bottomSwitchDevices.Add(bottomLANRouterDev.Get(1));

    //
    // Create bridge domains
    //

    BridgeHelper bridgeTop;
    bridgeTop.Install(topSwitches.Get(0), NetDeviceContainer(topSwitchDevices.Get(0), topEndpointDevices.Get(0)));

    BridgeHelper bridgeTop1;
    bridgeTop1.Install(topSwitches.Get(1), NetDeviceContainer(topSwitchDevices.Get(1), topEndpointDevices.Get(1)));

    BridgeHelper bridgeTop2;
    bridgeTop2.Install(topSwitches.Get(2), topSwitchDevices.Get(2));

    BridgeHelper bridgeTop3;
    bridgeTop3.Install(topSwitches.Get(3), topSwitchDevices.Get(3));

    BridgeHelper bridgeBottom;
    bridgeBottom.Install(bottomSwitches.Get(0), NetDeviceContainer(bottomSwitchDevices.Get(0), bottomEndpointDevices.Get(0)));

    BridgeHelper bridgeBottom1;
    bridgeBottom1.Install(bottomSwitches.Get(1), bottomSwitchDevices.Get(1));

    BridgeHelper bridgeBottom2;
    bridgeBottom2.Install(bottomSwitches.Get(2), bottomSwitchDevices.Get(2));

    BridgeHelper bridgeBottom3;
    bridgeBottom3.Install(bottomSwitches.Get(3), bottomSwitchDevices.Get(3));

    BridgeHelper bridgeBottom4;
    bridgeBottom4.Install(bottomSwitches.Get(4), bottomSwitchDevices.Get(4));


    //
    // Install Internet stack
    //
    InternetStackHelper internet;
    internet.Install(topRouters);
    internet.Install(bottomRouters);
    internet.Install(topEndpoints);
    internet.Install(bottomEndpoints);

    //
    // Assign addresses
    //
    Ipv4AddressHelper ipv4;

    // WAN
    ipv4.SetBase("76.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer wanInterfaces = ipv4.Assign(wanDevices);

    // Top LAN
    ipv4.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer topLANInterfaces = ipv4.Assign(topCsma.AssignIpv4Address(NodeContainer(topEndpoints), Ipv4Address("192.168.1.100"), Ipv4Mask("255.255.255.0")));
    Ipv4Address topRouterAddress("192.168.1.1"); // Assign an address to the router's LAN interface
    Ipv4InterfaceContainer topRouterInterfaces = ipv4.Assign(topLANRouterDev);
    Ipv4AddressHelper::AssignOne(topRouterInterfaces.Get(1), topRouterAddress, Ipv4Mask("255.255.255.0"));

    // Bottom LAN
    ipv4.SetBase("192.168.2.0", "255.255.255.0");
    Ipv4InterfaceContainer bottomLANInterfaces = ipv4.Assign(bottomCsma.AssignIpv4Address(NodeContainer(bottomEndpoints), Ipv4Address("192.168.2.100"), Ipv4Mask("255.255.255.0")));
    Ipv4Address bottomRouterAddress("192.168.2.1"); // Assign an address to the router's LAN interface
    Ipv4InterfaceContainer bottomRouterInterfaces = ipv4.Assign(bottomLANRouterDev);
    Ipv4AddressHelper::AssignOne(bottomRouterInterfaces.Get(1), bottomRouterAddress, Ipv4Mask("255.255.255.0"));


    //
    // Create static routes
    //
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    //
    // Create Applications
    //
    uint16_t echoPort = 9;

    // Top LAN client/server
    UdpEchoServerHelper echoServerTop(echoPort);
    ApplicationContainer serverAppTop = echoServerTop.Install(topEndpoints.Get(1));
    serverAppTop.Start(Seconds(1.0));
    serverAppTop.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClientTop(topLANInterfaces.GetAddress(1), echoPort);
    echoClientTop.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClientTop.SetAttribute("MaxPackets", UintegerValue(5));
    ApplicationContainer clientAppTop = echoClientTop.Install(topEndpoints.Get(0));
    clientAppTop.Start(Seconds(2.0));
    clientAppTop.Stop(Seconds(10.0));

    // Bottom LAN client/server
    UdpEchoServerHelper echoServerBottom(echoPort);
    ApplicationContainer serverAppBottom = echoServerBottom.Install(bottomEndpoints.Get(1));
    serverAppBottom.Start(Seconds(1.0));
    serverAppBottom.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClientBottom(bottomLANInterfaces.GetAddress(1), echoPort);
    echoClientBottom.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClientBottom.SetAttribute("MaxPackets", UintegerValue(5));
    ApplicationContainer clientAppBottom = echoClientBottom.Install(bottomEndpoints.Get(0));
    clientAppBottom.Start(Seconds(2.0));
    clientAppBottom.Stop(Seconds(10.0));


    //
    // Configure Tracing
    //
    if (tracing) {
        pointToPoint.EnablePcapAll("wan");
        topCsma.EnablePcap("top-lan", topSwitchDevices.Get(0), true);
        bottomCsma.EnablePcap("bottom-lan", bottomSwitchDevices.Get(0), true);
    }

    //
    // Run the simulation
    //
    Simulator::Stop(Seconds(11.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}