#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/dhcp-server-helper.h"
#include "ns3/vlan-helper.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable logging for DHCP and VLAN modules
    LogComponentEnable("DhcpClient", LOG_LEVEL_INFO);
    LogComponentEnable("DhcpServer", LOG_LEVEL_INFO);
    LogComponentEnable("VlanTaggedMac48AddressSink", LOG_LEVEL_INFO);

    // Create nodes: 6 hosts (2 per VLAN), 1 switch, 1 router, 1 DHCP server
    NodeContainer hosts;
    hosts.Create(6); // Hosts in VLAN 10, 20, 30

    Ptr<Node> l3Switch = CreateObject<Node>();
    Ptr<Node> router = CreateObject<Node>();
    Ptr<Node> dhcpServerNode = CreateObject<Node>();

    NodeContainer allNodes(hosts);
    allNodes.Add(l3Switch);
    allNodes.Add(router);
    allNodes.Add(dhcpServerNode);

    // Install internet stack on hosts, router, and DHCP server
    InternetStackHelper internet;
    internet.Install(hosts);
    internet.Install(router);
    internet.Install(dhcpServerNode);

    // Create point-to-point links between L3 switch and router
    PointToPointHelper p2pSwitchRouter;
    p2pSwitchRouter.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
    p2pSwitchRouter.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devSwitchRouter = p2pSwitchRouter.Install(l3Switch, router);

    // CSMA helper for connecting hosts to the switch
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("1Gbps"));
    csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));

    // VLAN configuration
    VlanHelper vlanHelper;

    // VLAN 10 - Subnet 192.168.10.0/24
    NetDeviceContainer devSwitchHostsVlan10 = csma.Install(NodeContainer(hosts.Get(0), hosts.Get(1), l3Switch));
    vlanHelper.AssignVlan(devSwitchHostsVlan10, 10);

    // VLAN 20 - Subnet 192.168.20.0/24
    NetDeviceContainer devSwitchHostsVlan20 = csma.Install(NodeContainer(hosts.Get(2), hosts.Get(3), l3Switch));
    vlanHelper.AssignVlan(devSwitchHostsVlan20, 20);

    // VLAN 30 - Subnet 192.168.30.0/24
    NetDeviceContainer devSwitchHostsVlan30 = csma.Install(NodeContainer(hosts.Get(4), hosts.Get(5), l3Switch));
    vlanHelper.AssignVlan(devSwitchHostsVlan30, 30);

    // Assign IP addresses to switch-router interfaces with VLAN tagging
    Ipv4AddressHelper ipSwitchRouter;
    ipSwitchRouter.SetBase("192.168.100.0", "255.255.255.0");

    Ipv4InterfaceContainer intSwitchRouter = ipSwitchRouter.Assign(devSwitchRouter);
    Ipv4Address routerIp = intSwitchRouter.GetAddress(1);

    // Set up inter-VLAN routing on router
    Ipv4StaticRoutingHelper ipv4RoutingHelper;
    Ptr<Ipv4StaticRouting> routing = ipv4RoutingHelper.GetStaticRouting(router->GetObject<Ipv4>());
    routing->AddNetworkRouteTo(Ipv4Address("192.168.10.0"), Ipv4Mask("255.255.255.0"), routerIp, 1);
    routing->AddNetworkRouteTo(Ipv4Address("192.168.20.0"), Ipv4Mask("255.255.255.0"), routerIp, 1);
    routing->AddNetworkRouteTo(Ipv4Address("192.168.30.0"), Ipv4Mask("255.255.255.0"), routerIp, 1);

    // Configure DHCP server
    DhcpServerHelper dhcpServerHelper;
    dhcpServerHelper.SetPool("192.168.10.0", "255.255.255.0", "192.168.10.100", "192.168.10.200");
    dhcpServerHelper.SetDefaultGateway("192.168.10.1");
    dhcpServerHelper.SetSubnetMask("255.255.255.0");

    NetDeviceContainer dhcpDevices;
    dhcpDevices.Add(csma.Install(NodeContainer(dhcpServerNode, l3Switch)));
    vlanHelper.AssignVlan(dhcpDevices, 10);

    Ipv4AddressHelper dhcpAddress;
    dhcpAddress.SetBase("192.168.10.0", "255.255.255.0");
    Ipv4InterfaceContainer dhcpInterfaces = dhcpAddress.Assign(dhcpDevices);

    ApplicationContainer dhcpApp = dhcpServerHelper.Install(dhcpServerNode);
    dhcpApp.Start(Seconds(1.0));
    dhcpApp.Stop(Seconds(20.0));

    // Configure DHCP clients on hosts
    DhcpClientHelper dhcpClientHelper;
    ApplicationContainer clientApps;

    for (uint32_t i = 0; i < hosts.GetN(); ++i) {
        NetDeviceContainer hostDevs;
        if (i < 2) {
            hostDevs = devSwitchHostsVlan10;
        } else if (i < 4) {
            hostDevs = devSwitchHostsVlan20;
        } else {
            hostDevs = devSwitchHostsVlan30;
        }

        // Install DHCP client
        ApplicationContainer dhcpClientApp = dhcpClientHelper.Install(hosts.Get(i));
        dhcpClientApp.Start(Seconds(2.0));
        dhcpClientApp.Stop(Seconds(20.0));
        clientApps.Add(dhcpClientApp);
    }

    // Generate traffic between hosts in different VLANs
    uint16_t port = 9; // Discard port
    OnOffHelper onoff("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address("192.168.20.101"), port));
    onoff.SetConstantRate(DataRate("1kbps"));
    onoff.SetAttribute("PacketSize", UintegerValue(512));

    ApplicationContainer app = onoff.Install(hosts.Get(0)); // Host from VLAN 10
    app.Start(Seconds(5.0));
    app.Stop(Seconds(20.0));

    PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApp = sink.Install(hosts.Get(2)); // Host from VLAN 20
    sinkApp.Start(Seconds(5.0));
    sinkApp.Stop(Seconds(20.0));

    // Enable PCAP tracing
    csma.EnablePcapAll("inter_vlan_dhcp");

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}