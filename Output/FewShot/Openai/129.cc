#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/dhcp-server-helper.h"
#include "ns3/dhcp-client-helper.h"
#include "ns3/bridge-module.h"
#include "ns3/packet-sink-helper.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // Enable DHCP logging for debugging and pcap
    LogComponentEnable("DhcpServer", LOG_LEVEL_INFO);
    LogComponentEnable("DhcpClient", LOG_LEVEL_INFO);

    // VLAN configuration
    const uint16_t VLAN1 = 10, VLAN2 = 20, VLAN3 = 30;

    // Subnets for each VLAN
    std::string subnet1 = "10.10.10.0";
    std::string subnet2 = "10.10.20.0";
    std::string subnet3 = "10.10.30.0";
    std::string netmask = "255.255.255.0";

    // Create hosts for each VLAN
    NodeContainer vlan1Hosts, vlan2Hosts, vlan3Hosts;
    vlan1Hosts.Create(2); // Hosts in VLAN10
    vlan2Hosts.Create(2); // Hosts in VLAN20
    vlan3Hosts.Create(2); // Hosts in VLAN30

    // Create switch (Layer 3, will act as VLAN aware switch)
    Ptr<Node> switchNode = CreateObject<Node>();

    // Create router node (inter-VLAN router)
    Ptr<Node> routerNode = CreateObject<Node>();

    // Connect hosts to the switch using CSMA
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("1Gbps"));
    csma.SetChannelAttribute("Delay", StringValue("1ms"));

    // Helper: Install VLAN interfaces between switch and each group of hosts
    NetDeviceContainer vlan1SwitchPorts, vlan2SwitchPorts, vlan3SwitchPorts;
    NetDeviceContainer vlan1HostDevs, vlan2HostDevs, vlan3HostDevs;

    // VLAN 10
    for (uint32_t i = 0; i < vlan1Hosts.GetN(); ++i) {
        NetDeviceContainer link = csma.Install(NodeContainer(vlan1Hosts.Get(i), switchNode));
        vlan1HostDevs.Add(link.Get(0));
        vlan1SwitchPorts.Add(link.Get(1));
    }
    // VLAN 20
    for (uint32_t i = 0; i < vlan2Hosts.GetN(); ++i) {
        NetDeviceContainer link = csma.Install(NodeContainer(vlan2Hosts.Get(i), switchNode));
        vlan2HostDevs.Add(link.Get(0));
        vlan2SwitchPorts.Add(link.Get(1));
    }
    // VLAN 30
    for (uint32_t i = 0; i < vlan3Hosts.GetN(); ++i) {
        NetDeviceContainer link = csma.Install(NodeContainer(vlan3Hosts.Get(i), switchNode));
        vlan3HostDevs.Add(link.Get(0));
        vlan3SwitchPorts.Add(link.Get(1));
    }

    // Trunk link between switch and router (supports all VLANs)
    NetDeviceContainer switchTrunk, routerTrunkDev;
    NetDeviceContainer trunkLink = csma.Install(NodeContainer(switchNode, routerNode));
    switchTrunk.Add(trunkLink.Get(0));
    routerTrunkDev.Add(trunkLink.Get(1));

    // Configure VLANs on the switch using BridgeNetDevice with VLAN filtering
    BridgeHelper bridgeHelper;

    // Add VLAN10 ports
    NetDeviceContainer vlan10Ports = vlan1SwitchPorts;
    // Add VLAN20 ports
    NetDeviceContainer vlan20Ports = vlan2SwitchPorts;
    // Add VLAN30 ports
    NetDeviceContainer vlan30Ports = vlan3SwitchPorts;

    // Add all access and trunk ports to switch bridge
    NetDeviceContainer allSwitchPorts;
    allSwitchPorts.Add(vlan1SwitchPorts);
    allSwitchPorts.Add(vlan2SwitchPorts);
    allSwitchPorts.Add(vlan3SwitchPorts);
    allSwitchPorts.Add(switchTrunk);

    // Configure VLANs on each access port
    for (uint32_t i = 0; i < vlan1SwitchPorts.GetN(); ++i)
        vlan1SwitchPorts.Get(i)->SetAttribute("VlanId", UintegerValue(VLAN1));
    for (uint32_t i = 0; i < vlan2SwitchPorts.GetN(); ++i)
        vlan2SwitchPorts.Get(i)->SetAttribute("VlanId", UintegerValue(VLAN2));
    for (uint32_t i = 0; i < vlan3SwitchPorts.GetN(); ++i)
        vlan3SwitchPorts.Get(i)->SetAttribute("VlanId", UintegerValue(VLAN3));

    // Bridge all switch ports with VLAN support enabled
    Ptr<BridgeNetDevice> bridge = CreateObject<BridgeNetDevice>();
    bridge->SetAttribute("EnableVlanFiltering", BooleanValue(true));
    for (uint32_t i = 0; i < allSwitchPorts.GetN(); ++i)
        bridge->AddBridgePort(allSwitchPorts.Get(i));
    switchNode->AddDevice(bridge);

    // Configure VLAN subinterfaces on router trunk (Router-on-a-stick)
    NetDeviceContainer routerVlanDevs;
    Ptr<NetDevice> trunk = routerTrunkDev.Get(0);

    // VLAN10 subinterface
    Ptr<VlanNetDevice> routerVlan10 = CreateObject<VlanNetDevice>();
    routerVlan10->SetAttribute("VlanId", UintegerValue(VLAN1));
    routerVlan10->SetAttribute("TrunkMode", BooleanValue(true));
    routerVlan10->SetUnderlying(trunk);
    routerNode->AddDevice(routerVlan10);
    routerVlanDevs.Add(routerVlan10);

    // VLAN20 subinterface
    Ptr<VlanNetDevice> routerVlan20 = CreateObject<VlanNetDevice>();
    routerVlan20->SetAttribute("VlanId", UintegerValue(VLAN2));
    routerVlan20->SetAttribute("TrunkMode", BooleanValue(true));
    routerVlan20->SetUnderlying(trunk);
    routerNode->AddDevice(routerVlan20);
    routerVlanDevs.Add(routerVlan20);

    // VLAN30 subinterface
    Ptr<VlanNetDevice> routerVlan30 = CreateObject<VlanNetDevice>();
    routerVlan30->SetAttribute("VlanId", UintegerValue(VLAN3));
    routerVlan30->SetAttribute("TrunkMode", BooleanValue(true));
    routerVlan30->SetUnderlying(trunk);
    routerNode->AddDevice(routerVlan30);
    routerVlanDevs.Add(routerVlan30);

    // Install Internet stack on all nodes
    InternetStackHelper stack;
    stack.Install(vlan1Hosts);
    stack.Install(vlan2Hosts);
    stack.Install(vlan3Hosts);
    stack.Install(routerNode);

    // Set up DHCP server helper on router for each VLAN
    // Helper: Set router VLAN interfaces as the server interface for each VLAN
    DhcpServerHelper dhcpServer10(routerVlanDevs.Get(0), Ipv4Address(subnet1.c_str()), Ipv4Mask(netmask.c_str()), Ipv4Address("10.10.10.1"));
    DhcpServerHelper dhcpServer20(routerVlanDevs.Get(1), Ipv4Address(subnet2.c_str()), Ipv4Mask(netmask.c_str()), Ipv4Address("10.10.20.1"));
    DhcpServerHelper dhcpServer30(routerVlanDevs.Get(2), Ipv4Address(subnet3.c_str()), Ipv4Mask(netmask.c_str()), Ipv4Address("10.10.30.1"));

    ApplicationContainer dhcpServers;
    dhcpServers.Add(dhcpServer10.Install(routerNode));
    dhcpServers.Add(dhcpServer20.Install(routerNode));
    dhcpServers.Add(dhcpServer30.Install(routerNode));
    dhcpServers.Start(Seconds(0.1));
    dhcpServers.Stop(Seconds(20.0));

    // Enable pcap tracing for DHCP
    csma.EnablePcapAll("dhcp-vlan", false, true);

    // Assign IPs dynamically via DHCP clients on each host
    DhcpClientHelper dhcpClient;
    std::vector<ApplicationContainer> dhcpClients;
    for (uint32_t i = 0; i < vlan1Hosts.GetN(); ++i)
        dhcpClients.push_back(dhcpClient.Install(vlan1Hosts.Get(i), vlan1HostDevs.Get(i)));
    for (uint32_t i = 0; i < vlan2Hosts.GetN(); ++i)
        dhcpClients.push_back(dhcpClient.Install(vlan2Hosts.Get(i), vlan2HostDevs.Get(i)));
    for (uint32_t i = 0; i < vlan3Hosts.GetN(); ++i)
        dhcpClients.push_back(dhcpClient.Install(vlan3Hosts.Get(i), vlan3HostDevs.Get(i)));
    for (auto &apps : dhcpClients) {
        apps.Start(Seconds(0.5));
        apps.Stop(Seconds(20.0));
    }

    // Assign static IPs to router VLAN interfaces
    Ipv4AddressHelper ipv4;
    Ptr<Ipv4> ipv4Router = routerNode->GetObject<Ipv4>();
    ipv4.SetBase(subnet1.c_str(), netmask.c_str());
    int32_t ifVlan10 = routerNode->GetObject<Ipv4>()->AddInterface(routerVlanDevs.Get(0));
    ipv4Router->AddAddress(ifVlan10, Ipv4InterfaceAddress("10.10.10.1", netmask.c_str()));
    ipv4Router->SetUp(ifVlan10);

    ipv4.SetBase(subnet2.c_str(), netmask.c_str());
    int32_t ifVlan20 = routerNode->GetObject<Ipv4>()->AddInterface(routerVlanDevs.Get(1));
    ipv4Router->AddAddress(ifVlan20, Ipv4InterfaceAddress("10.10.20.1", netmask.c_str()));
    ipv4Router->SetUp(ifVlan20);

    ipv4.SetBase(subnet3.c_str(), netmask.c_str());
    int32_t ifVlan30 = routerNode->GetObject<Ipv4>()->AddInterface(routerVlanDevs.Get(2));
    ipv4Router->AddAddress(ifVlan30, Ipv4InterfaceAddress("10.10.30.1", netmask.c_str()));
    ipv4Router->SetUp(ifVlan30);

    // Install applications to send inter-VLAN traffic (UDP Echo)
    uint16_t echoPort = 9000;

    // Launch UDP Echo servers on each host in VLAN3
    UdpEchoServerHelper echoServer(echoPort);
    ApplicationContainer serverApps;
    for (uint32_t i = 0; i < vlan3Hosts.GetN(); ++i)
    {
        serverApps.Add(echoServer.Install(vlan3Hosts.Get(i)));
    }
    serverApps.Start(Seconds(3.0));
    serverApps.Stop(Seconds(18.0));

    // Launch UDP Echo clients on VLAN1 host[0] to send echo to VLAN3 hosts
    ApplicationContainer clientApps;
    for (uint32_t i = 0; i < vlan3Hosts.GetN(); ++i)
    {
        UdpEchoClientHelper echoClient(Ipv4Address("10.10.30." + std::to_string(2 + i)), echoPort);
        echoClient.SetAttribute("MaxPackets", UintegerValue(5));
        echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
        echoClient.SetAttribute("PacketSize", UintegerValue(128));
        clientApps.Add(echoClient.Install(vlan1Hosts.Get(0)));
    }
    clientApps.Start(Seconds(5.0));
    clientApps.Stop(Seconds(17.0));

    // Enable routing on the router
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    Simulator::Stop(Seconds(20.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}