#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/dhcp-helper.h"
#include "ns3/csma-module.h"
#include "ns3/vlan-tag.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable logging
    LogComponentEnable("DhcpServer", LOG_LEVEL_INFO);
    LogComponentEnable("DhcpClient", LOG_LEVEL_INFO);

    // Create nodes: Router, Switch, VLAN1 Host, VLAN2 Host, VLAN3 Host
    NodeContainer routerNode;
    routerNode.Create(1);
    NodeContainer switchNode;
    switchNode.Create(1);
    NodeContainer vlan1Node;
    vlan1Node.Create(1);
    NodeContainer vlan2Node;
    vlan2Node.Create(1);
    NodeContainer vlan3Node;
    vlan3Node.Create(1);

    // Create the VLAN network topology
    CsmaHelper csmaVlan1;
    csmaVlan1.SetChannelAttribute("DataRate", StringValue("10Mbps"));
    csmaVlan1.SetChannelAttribute("Delay", StringValue("2ms"));
    NetDeviceContainer switchVlan1Devices = csmaVlan1.Install(NodeContainer(switchNode, vlan1Node));

    CsmaHelper csmaVlan2;
    csmaVlan2.SetChannelAttribute("DataRate", StringValue("10Mbps"));
    csmaVlan2.SetChannelAttribute("Delay", StringValue("2ms"));
    NetDeviceContainer switchVlan2Devices = csmaVlan2.Install(NodeContainer(switchNode, vlan2Node));

    CsmaHelper csmaVlan3;
    csmaVlan3.SetChannelAttribute("DataRate", StringValue("10Mbps"));
    csmaVlan3.SetChannelAttribute("Delay", StringValue("2ms"));
    NetDeviceContainer switchVlan3Devices = csmaVlan3.Install(NodeContainer(switchNode, vlan3Node));

    // Router connection to each VLAN
    PointToPointHelper p2pRouterVlan1;
    p2pRouterVlan1.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2pRouterVlan1.SetChannelAttribute("Delay", StringValue("1ms"));
    NetDeviceContainer routerVlan1Devices = p2pRouterVlan1.Install(NodeContainer(routerNode, switchNode));

    PointToPointHelper p2pRouterVlan2;
    p2pRouterVlan2.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2pRouterVlan2.SetChannelAttribute("Delay", StringValue("1ms"));
    NetDeviceContainer routerVlan2Devices = p2pRouterVlan2.Install(NodeContainer(routerNode, switchNode));

    PointToPointHelper p2pRouterVlan3;
    p2pRouterVlan3.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2pRouterVlan3.SetChannelAttribute("Delay", StringValue("1ms"));
    NetDeviceContainer routerVlan3Devices = p2pRouterVlan3.Install(NodeContainer(routerNode, switchNode));


    // Install Internet stack on all nodes
    InternetStackHelper internet;
    internet.Install(NodeContainer(routerNode, switchNode, vlan1Node, vlan2Node, vlan3Node));

    // Assign IP addresses to the VLANs
    Ipv4AddressHelper address;

    // VLAN1
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer vlan1Interfaces = address.Assign(switchVlan1Devices);

    // VLAN2
    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer vlan2Interfaces = address.Assign(switchVlan2Devices);

    // VLAN3
    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer vlan3Interfaces = address.Assign(switchVlan3Devices);

    // Router Interfaces
    Ipv4AddressHelper routerAddress;
    routerAddress.SetBase("10.1.4.0", "255.255.255.0");
    Ipv4InterfaceContainer routerVlan1Interface = routerAddress.Assign(routerVlan1Devices);
    routerAddress.SetBase("10.1.5.0", "255.255.255.0");
    Ipv4InterfaceContainer routerVlan2Interface = routerAddress.Assign(routerVlan2Devices);
    routerAddress.SetBase("10.1.6.0", "255.255.255.0");
    Ipv4InterfaceContainer routerVlan3Interface = routerAddress.Assign(routerVlan3Devices);

    // Configure DHCP Server on each VLAN
    DhcpServerHelper dhcpServerVlan1;
    dhcpServerVlan1.SetAddressPool(Ipv4Address("10.1.1.10"), Ipv4Address("10.1.1.20"), Ipv4Mask("255.255.255.0"));
    dhcpServerVlan1.Install(switchNode.Get(0), switchVlan1Devices.Get(0));
    dhcpServerVlan1.EnablePcap("dhcp-vlan1", switchVlan1Devices.Get(0), true);

    DhcpServerHelper dhcpServerVlan2;
    dhcpServerVlan2.SetAddressPool(Ipv4Address("10.1.2.10"), Ipv4Address("10.1.2.20"), Ipv4Mask("255.255.255.0"));
    dhcpServerVlan2.Install(switchNode.Get(0), switchVlan2Devices.Get(0));
    dhcpServerVlan2.EnablePcap("dhcp-vlan2", switchVlan2Devices.Get(0), true);

    DhcpServerHelper dhcpServerVlan3;
    dhcpServerVlan3.SetAddressPool(Ipv4Address("10.1.3.10"), Ipv4Address("10.1.3.20"), Ipv4Mask("255.255.255.0"));
    dhcpServerVlan3.Install(switchNode.Get(0), switchVlan3Devices.Get(0));
    dhcpServerVlan3.EnablePcap("dhcp-vlan3", switchVlan3Devices.Get(0), true);

    // Configure DHCP Client on each VLAN Host
    DhcpClientHelper dhcpClientVlan1;
    dhcpClientVlan1.Install(vlan1Node.Get(0), switchVlan1Devices.Get(1));

    DhcpClientHelper dhcpClientVlan2;
    dhcpClientVlan2.Install(vlan2Node.Get(0), switchVlan2Devices.Get(1));

    DhcpClientHelper dhcpClientVlan3;
    dhcpClientVlan3.Install(vlan3Node.Get(0), switchVlan3Devices.Get(1));

    // Enable IPv4 global routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Set up traffic between VLANs
    UdpEchoClientHelper echoClient(vlan2Interfaces.GetAddress(1), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp = echoClient.Install(vlan1Node.Get(0));
    clientApp.Start(Seconds(5.0));
    clientApp.Stop(Seconds(15.0));

    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApp = echoServer.Install(vlan2Node.Get(0));
    serverApp.Start(Seconds(5.0));
    serverApp.Stop(Seconds(15.0));

    // Run the simulation
    Simulator::Stop(Seconds(20.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}