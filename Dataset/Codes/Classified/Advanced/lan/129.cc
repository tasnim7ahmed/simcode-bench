#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/dhcp-helper.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/point-to-point-module.h"
#include "ns3/bridge-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("InterVLANRoutingWithDHCP");

int main(int argc, char *argv[])
{
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create 6 nodes: 1 router, 2 nodes for each of the 3 VLANs
    NodeContainer router;
    router.Create(1); // Router node

    NodeContainer vlan1Hosts, vlan2Hosts, vlan3Hosts;
    vlan1Hosts.Create(2);
    vlan2Hosts.Create(2);
    vlan3Hosts.Create(2);

    // Create point-to-point links between the router and each VLAN switch
    NodeContainer vlan1Switch, vlan2Switch, vlan3Switch;
    vlan1Switch.Create(1);
    vlan2Switch.Create(1);
    vlan3Switch.Create(1);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer vlan1RouterDevices, vlan2RouterDevices, vlan3RouterDevices;
    vlan1RouterDevices = p2p.Install(router.Get(0), vlan1Switch.Get(0));
    vlan2RouterDevices = p2p.Install(router.Get(0), vlan2Switch.Get(0));
    vlan3RouterDevices = p2p.Install(router.Get(0), vlan3Switch.Get(0));

    // Add bridges for the VLANs
    NetDeviceContainer vlan1Devices, vlan2Devices, vlan3Devices;
    BridgeHelper bridge;
    vlan1Devices.Add(vlan1RouterDevices.Get(1));
    vlan2Devices.Add(vlan2RouterDevices.Get(1));
    vlan3Devices.Add(vlan3RouterDevices.Get(1));

    NetDeviceContainer vlan1HostDevices = bridge.Install(vlan1Switch.Get(0), vlan1Devices);
    NetDeviceContainer vlan2HostDevices = bridge.Install(vlan2Switch.Get(0), vlan2Devices);
    NetDeviceContainer vlan3HostDevices = bridge.Install(vlan3Switch.Get(0), vlan3Devices);

    // Install Internet Stack
    InternetStackHelper internet;
    internet.Install(router);
    internet.Install(vlan1Hosts);
    internet.Install(vlan2Hosts);
    internet.Install(vlan3Hosts);

    // Assign IP addresses to the router's interfaces
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer vlan1Interfaces = ipv4.Assign(vlan1RouterDevices.Get(0));

    ipv4.SetBase("192.168.2.0", "255.255.255.0");
    Ipv4InterfaceContainer vlan2Interfaces = ipv4.Assign(vlan2RouterDevices.Get(0));

    ipv4.SetBase("192.168.3.0", "255.255.255.0");
    Ipv4InterfaceContainer vlan3Interfaces = ipv4.Assign(vlan3RouterDevices.Get(0));

    // Create DHCP server and client configuration
    DhcpHelper dhcp;
    dhcp.SetRouter(router.Get(0));

    dhcp.InstallDhcpServer(vlan1RouterDevices.Get(1), Ipv4Address("192.168.1.1"));
    dhcp.InstallDhcpServer(vlan2RouterDevices.Get(1), Ipv4Address("192.168.2.1"));
    dhcp.InstallDhcpServer(vlan3RouterDevices.Get(1), Ipv4Address("192.168.3.1"));

    dhcp.InstallDhcpClient(vlan1HostDevices);
    dhcp.InstallDhcpClient(vlan2HostDevices);
    dhcp.InstallDhcpClient(vlan3HostDevices);

    // Enable pcap tracing to capture DHCP traffic
    p2p.EnablePcapAll("inter-vlan-dhcp");

    // Create UDP Echo server on one of the VLAN3 hosts
    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApp = echoServer.Install(vlan3Hosts.Get(1));
    serverApp.Start(Seconds(2.0));
    serverApp.Stop(Seconds(10.0));

    // Create a UDP Echo client on VLAN1 to send requests to the server in VLAN3
    UdpEchoClientHelper echoClient(vlan3Interfaces.GetAddress(1), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp = echoClient.Install(vlan1Hosts.Get(0));
    clientApp.Start(Seconds(3.0));
    clientApp.Stop(Seconds(10.0));

    // Configure routing on the router for inter-VLAN routing
    Ipv4StaticRoutingHelper ipv4RoutingHelper;
    Ptr<Ipv4StaticRouting> routerStaticRouting = ipv4RoutingHelper.GetStaticRouting(router.Get(0)->GetObject<Ipv4>());
    routerStaticRouting->SetDefaultRoute("192.168.1.1", 1);
    routerStaticRouting->SetDefaultRoute("192.168.2.1", 2);
    routerStaticRouting->SetDefaultRoute("192.168.3.1", 3);

    // Run the simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}

