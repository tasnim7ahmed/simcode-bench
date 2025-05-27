#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/csma-module.h"
#include "ns3/bridge-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    bool tracing = false;
    std::string csmaSpeed = "100Mbps";

    CommandLine cmd;
    cmd.AddValue("tracing", "Enable PCAP tracing", tracing);
    cmd.AddValue("csmaSpeed", "CSMA link speed (10Mbps or 100Mbps)", csmaSpeed);
    cmd.Parse(argc, argv);

    // Top LAN setup
    NodeContainer topRouters, topEndpoints;
    topRouters.Create(1);
    topEndpoints.Create(2);

    NodeContainer topSwitches;
    topSwitches.Create(4);

    CsmaHelper topCsma;
    topCsma.SetChannelAttribute("DataRate", StringValue(csmaSpeed));
    topCsma.SetChannelAttribute("Delay", TimeValue(MicroSeconds(2)));

    NetDeviceContainer topRouterDevices = topCsma.Install(NodeContainer(topRouters.Get(0), topSwitches.Get(0)));

    NetDeviceContainer ts1Devices = topCsma.Install(NodeContainer(topSwitches.Get(0), topSwitches.Get(1)));
    NetDeviceContainer ts2Devices = topCsma.Install(NodeContainer(topSwitches.Get(1), topSwitches.Get(2)));
    NetDeviceContainer ts3Devices = topCsma.Install(NodeContainer(topSwitches.Get(2), topSwitches.Get(3)));
    NetDeviceContainer ts4Devices = topCsma.Install(NodeContainer(topSwitches.Get(3), topEndpoints.Get(0)));
    NetDeviceContainer t3Devices = topCsma.Install(NodeContainer(topSwitches.Get(0), topEndpoints.Get(1)));

    BridgeHelper topBridge;
    topBridge.Install(topSwitches.Get(0), {topRouterDevices.Get(1), ts1Devices.Get(0), t3Devices.Get(0)});
    topBridge.Install(topSwitches.Get(1), {ts1Devices.Get(1), ts2Devices.Get(0)});
    topBridge.Install(topSwitches.Get(2), {ts2Devices.Get(1), ts3Devices.Get(0)});
    topBridge.Install(topSwitches.Get(3), {ts3Devices.Get(1), ts4Devices.Get(0)});

    // Bottom LAN setup
    NodeContainer bottomRouters, bottomEndpoints;
    bottomRouters.Create(1);
    bottomEndpoints.Create(2);

    NodeContainer bottomSwitches;
    bottomSwitches.Create(5);

    CsmaHelper bottomCsma;
    bottomCsma.SetChannelAttribute("DataRate", StringValue(csmaSpeed));
    bottomCsma.SetChannelAttribute("Delay", TimeValue(MicroSeconds(2)));

    NetDeviceContainer bottomRouterDevices = bottomCsma.Install(NodeContainer(bottomRouters.Get(0), bottomSwitches.Get(0)));

    NetDeviceContainer bs1Devices = bottomCsma.Install(NodeContainer(bottomSwitches.Get(0), bottomSwitches.Get(1)));
    NetDeviceContainer bs2Devices = bottomCsma.Install(NodeContainer(bottomSwitches.Get(1), bottomSwitches.Get(2)));
    NetDeviceContainer bs3Devices = bottomCsma.Install(NodeContainer(bottomSwitches.Get(2), bottomSwitches.Get(3)));
    NetDeviceContainer bs4Devices = bottomCsma.Install(NodeContainer(bottomSwitches.Get(3), bottomSwitches.Get(4)));
    NetDeviceContainer bs5Devices = bottomCsma.Install(NodeContainer(bottomSwitches.Get(4), bottomEndpoints.Get(0)));
    NetDeviceContainer b3Devices = bottomCsma.Install(NodeContainer(bottomSwitches.Get(0), bottomEndpoints.Get(1)));

    BridgeHelper bottomBridge;
    bottomBridge.Install(bottomSwitches.Get(0), {bottomRouterDevices.Get(1), bs1Devices.Get(0), b3Devices.Get(0)});
    bottomBridge.Install(bottomSwitches.Get(1), {bs1Devices.Get(1), bs2Devices.Get(0)});
    bottomBridge.Install(bottomSwitches.Get(2), {bs2Devices.Get(1), bs3Devices.Get(0)});
    bottomBridge.Install(bottomSwitches.Get(3), {bs3Devices.Get(1), bs4Devices.Get(0)});
    bottomBridge.Install(bottomSwitches.Get(4), {bs4Devices.Get(1), bs5Devices.Get(0)});

    // WAN link
    PointToPointHelper wanPointToPoint;
    wanPointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    wanPointToPoint.SetChannelAttribute("Delay", StringValue("50ms"));

    NetDeviceContainer wanDevices = wanPointToPoint.Install(NodeContainer(topRouters.Get(0), bottomRouters.Get(0)));

    // Install Internet stack
    InternetStackHelper stack;
    stack.InstallAll();

    // Assign IP addresses
    Ipv4AddressHelper address;

    // Top LAN: 192.168.1.0/24
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer topRouterInterfaces = address.Assign(topRouterDevices);
    address.Assign(t3Devices);
    Ipv4InterfaceContainer topEndpointInterfaces = address.Assign(ts4Devices);

    // Bottom LAN: 192.168.2.0/24
    address.SetBase("192.168.2.0", "255.255.255.0");
    Ipv4InterfaceContainer bottomRouterInterfaces = address.Assign(bottomRouterDevices);
    address.Assign(b3Devices);
    Ipv4InterfaceContainer bottomEndpointInterfaces = address.Assign(bs5Devices);

    // WAN: 76.1.1.0/30
    address.SetBase("76.1.1.0", "255.255.255.252");
    Ipv4InterfaceContainer wanInterfaces = address.Assign(wanDevices);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // UDP echo applications
    uint16_t port = 9;

    UdpEchoServerHelper echoServerTop(port);
    ApplicationContainer serverAppTop = echoServerTop.Install(topEndpoints.Get(0));
    serverAppTop.Start(Seconds(1.0));
    serverAppTop.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClientTop(topEndpointInterfaces.GetAddress(0), port);
    echoClientTop.SetAttribute("MaxPackets", UintegerValue(5));
    echoClientTop.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClientTop.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientAppTop = echoClientTop.Install(bottomEndpoints.Get(0));
    clientAppTop.Start(Seconds(2.0));
    clientAppTop.Stop(Seconds(10.0));

    UdpEchoServerHelper echoServerBottom(port);
    ApplicationContainer serverAppBottom = echoServerBottom.Install(bottomEndpoints.Get(1));
    serverAppBottom.Start(Seconds(1.0));
    serverAppBottom.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClientBottom(bottomEndpointInterfaces.GetAddress(0), port);
    echoClientBottom.SetAttribute("MaxPackets", UintegerValue(5));
    echoClientBottom.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClientBottom.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientAppBottom = echoClientBottom.Install(topEndpoints.Get(1));
    clientAppBottom.Start(Seconds(2.0));
    clientAppBottom.Stop(Seconds(10.0));


    // Tracing
    if (tracing) {
        topCsma.EnablePcap("top-lan", topRouterDevices.Get(0), true);
        bottomCsma.EnablePcap("bottom-lan", bottomRouterDevices.Get(0), true);
        wanPointToPoint.EnablePcapAll("wan");
    }

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}