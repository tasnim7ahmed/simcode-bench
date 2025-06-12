#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/csma-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"
#include "ns3/dhcp-server-helper.h"
#include "ns3/vlan-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("InterVlanRoutingDhcpSimulation");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
    LogComponentEnable("DhcpClient", LOG_LEVEL_ALL);
    LogComponentEnable("DhcpServer", LOG_LEVEL_ALL);

    NodeContainer hosts;
    hosts.Create(6);

    NodeContainer switchNode;
    switchNode.Create(1);

    NodeContainer router;
    router.Create(1);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    CsmaHelper csmaSwitch;
    csmaSwitch.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csmaSwitch.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));

    NetDeviceContainer switchDevices;
    switchDevices = csmaSwitch.Install(switchNode.Get(0)->GetDevice(0));

    VlanHelper vlanHelper;

    // VLAN 10
    NodeContainer vlan10Hosts;
    vlan10Hosts.Add(hosts.Get(0));
    vlan10Hosts.Add(hosts.Get(1));
    NetDeviceContainer vlan10Devs = p2p.Install(switchNode.Get(0), vlan10Hosts);
    vlanHelper.AssignVlan(vlan10Devs, 10);

    // VLAN 20
    NodeContainer vlan20Hosts;
    vlan20Hosts.Add(hosts.Get(2));
    vlan20Hosts.Add(hosts.Get(3));
    NetDeviceContainer vlan20Devs = p2p.Install(switchNode.Get(0), vlan20Hosts);
    vlanHelper.AssignVlan(vlan20Devs, 20);

    // VLAN 30
    NodeContainer vlan30Hosts;
    vlan30Hosts.Add(hosts.Get(4));
    vlan30Hosts.Add(hosts.Get(5));
    NetDeviceContainer vlan30Devs = p2p.Install(switchNode.Get(0), vlan30Hosts);
    vlanHelper.AssignVlan(vlan30Devs, 30);

    // Router connection to Layer 3 switch (trunk link)
    NetDeviceContainer routerSwitchDevs = p2p.Install(router.Get(0), switchNode.Get(0));

    InternetStackHelper internet;
    internet.InstallAll();

    DhcpServerHelper dhcpServerHelper;

    Ipv4AddressHelper address;

    // VLAN 10 subnet
    address.SetBase("192.168.10.0", "255.255.255.0");
    Ipv4InterfaceContainer vlan10Interfaces = address.Assign(vlan10Devs);
    dhcpServerHelper.SetLeasePool("192.168.10.100", "255.255.255.0", "192.168.10.200", "192.168.10.250");
    dhcpServerHelper.SetDefaultGateway("192.168.10.1");
    dhcpServerHelper.Install(hosts.Get(0));
    dhcpServerHelper.Install(hosts.Get(1));

    // VLAN 20 subnet
    address.SetBase("192.168.20.0", "255.255.255.0");
    Ipv4InterfaceContainer vlan20Interfaces = address.Assign(vlan20Devs);
    dhcpServerHelper.SetLeasePool("192.168.20.100", "255.255.255.0", "192.168.20.200", "192.168.20.250");
    dhcpServerHelper.SetDefaultGateway("192.168.20.1");
    dhcpServerHelper.Install(hosts.Get(2));
    dhcpServerHelper.Install(hosts.Get(3));

    // VLAN 30 subnet
    address.SetBase("192.168.30.0", "255.255.255.0");
    Ipv4InterfaceContainer vlan30Interfaces = address.Assign(vlan30Devs);
    dhcpServerHelper.SetLeasePool("192.168.30.100", "255.255.255.0", "192.168.30.200", "192.168.30.250");
    dhcpServerHelper.SetDefaultGateway("192.168.30.1");
    dhcpServerHelper.Install(hosts.Get(4));
    dhcpServerHelper.Install(hosts.Get(5));

    // Assign IP addresses to router interfaces for each VLAN on trunk link
    Ipv4InterfaceContainer routerInterfaces;

    // VLAN 10 interface on router
    Ptr<NetDevice> routerDev10 = routerSwitchDevs.Get(0);
    vlanHelper.AssignVlan(routerDev10, 10);
    address.SetBase("192.168.10.0", "255.255.255.0");
    routerInterfaces.Add(address.Assign(routerDev10));

    // VLAN 20 interface on router
    Ptr<NetDevice> routerDev20 = routerSwitchDevs.Get(0);
    vlanHelper.AssignVlan(routerDev20, 20);
    address.SetBase("192.168.20.0", "255.255.255.0");
    routerInterfaces.Add(address.Assign(routerDev20));

    // VLAN 30 interface on router
    Ptr<NetDevice> routerDev30 = routerSwitchDevs.Get(0);
    vlanHelper.AssignVlan(routerDev30, 30);
    address.SetBase("192.168.30.0", "255.255.255.0");
    routerInterfaces.Add(address.Assign(routerDev30));

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Enable pcap tracing
    p2p.EnablePcapAll("inter-vlan-dhcp");

    // UDP Echo Server setup on one host from each VLAN
    UdpEchoServerHelper echoServer(9);

    ApplicationContainer serverApps;
    serverApps.Add(echoServer.Install(hosts.Get(1)));
    serverApps.Add(echoServer.Install(hosts.Get(3)));
    serverApps.Add(echoServer.Install(hosts.Get(5)));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // UDP Echo Clients from other VLANs
    UdpEchoClientHelper echoClient(hosts.Get(1)->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal(), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps;
    clientApps.Add(echoClient.Install(hosts.Get(4))); // From VLAN 30 to VLAN 10 host
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    Simulator::Run();
    Simulator::Destroy();
    return 0;
}