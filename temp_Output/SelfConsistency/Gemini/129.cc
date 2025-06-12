#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/switch-module.h"
#include "ns3/dhcp-server-helper.h"
#include "ns3/dhcpv4-client-module.h"
#include "ns3/packet-sink.h"
#include "ns3/packet-sink-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("VlanRouting");

int main(int argc, char *argv[]) {
    CommandLine cmd;
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponent::SetCategory("DhcpServer", LOG_LEVEL_ALL);
    LogComponent::SetCategory("DhcpClient", LOG_LEVEL_ALL);

    NodeContainer router, sw, hostsVlan10, hostsVlan20, hostsVlan30;
    router.Create(1);
    sw.Create(1);
    hostsVlan10.Create(2);
    hostsVlan20.Create(2);
    hostsVlan30.Create(2);

    // Create a container for all nodes
    NodeContainer allNodes;
    allNodes.Add(router);
    allNodes.Add(sw);
    allNodes.Add(hostsVlan10);
    allNodes.Add(hostsVlan20);
    allNodes.Add(hostsVlan30);

    // PointToPoint links between router and switch
    PointToPointHelper p2pRouterSwitch;
    p2pRouterSwitch.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
    p2pRouterSwitch.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer routerSwDevs = p2pRouterSwitch.Install(router.Get(0), sw.Get(0));

    // PointToPoint links between switch and hosts
    PointToPointHelper p2pSwitchHosts;
    p2pSwitchHosts.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2pSwitchHosts.SetChannelAttribute("Delay", StringValue("1ms"));

    NetDeviceContainer switchVlan10Devs = p2pSwitchHosts.Install(sw.Get(0), hostsVlan10);
    NetDeviceContainer switchVlan20Devs = p2pSwitchHosts.Install(sw.Get(0), hostsVlan20);
    NetDeviceContainer switchVlan30Devs = p2pSwitchHosts.Install(sw.Get(0), hostsVlan30);

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(allNodes);

    // Configure VLANs on the switch
    VlanTag tag10 = 10;
    VlanTag tag20 = 20;
    VlanTag tag30 = 30;

    SwitchHelper switchHelper;
    switchHelper.ConfigureVlan(sw.Get(0), 0, tag10); // Router port
    switchHelper.ConfigureVlan(sw.Get(0), 1, tag10);
    switchHelper.ConfigureVlan(sw.Get(0), 2, tag10);
    switchHelper.ConfigureVlan(sw.Get(0), 3, tag20);
    switchHelper.ConfigureVlan(sw.Get(0), 4, tag20);
    switchHelper.ConfigureVlan(sw.Get(0), 5, tag30);
    switchHelper.ConfigureVlan(sw.Get(0), 6, tag30);

    // Assign IP addresses to router interfaces
    Ipv4AddressHelper address;
    address.SetBase("10.1.10.0", "255.255.255.0");
    Ipv4InterfaceContainer routerSwIfaces = address.Assign(routerSwDevs);
    Ipv4Address routerVlan10Addr = "10.1.10.1";
    Ipv4Address routerVlan20Addr = "10.1.20.1";
    Ipv4Address routerVlan30Addr = "10.1.30.1";

    // Configure router interfaces' IP addresses
    Ptr<Node> routerNode = router.Get(0);
    Ptr<NetDevice> routerSwDevice = routerSwDevs.Get(0);
    int32_t interfaceIndex = routerNode->GetId();

    // Create DHCP servers for each VLAN
    DhcpServerHelper dhcpServerHelper;
    Ipv4Address dhcpVlan10Start = "10.1.10.10";
    Ipv4Address dhcpVlan20Start = "10.1.20.10";
    Ipv4Address dhcpVlan30Start = "10.1.30.10";

    dhcpServerHelper.EnablePcap("dhcp_vlan10", switchVlan10Devs.Get(0), true);
    dhcpServerHelper.EnablePcap("dhcp_vlan20", switchVlan20Devs.Get(0), true);
    dhcpServerHelper.EnablePcap("dhcp_vlan30", switchVlan30Devs.Get(0), true);

    dhcpServerHelper.SetAddressPool(Ipv4AddressHelper(dhcpVlan10Start, "255.255.255.0"), 10);
    dhcpServerHelper.Install(hostsVlan10.Get(0));

    dhcpServerHelper.SetAddressPool(Ipv4AddressHelper(dhcpVlan20Start, "255.255.255.0"), 10);
    dhcpServerHelper.Install(hostsVlan20.Get(0));

    dhcpServerHelper.SetAddressPool(Ipv4AddressHelper(dhcpVlan30Start, "255.255.255.0"), 10);
    dhcpServerHelper.Install(hostsVlan30.Get(0));

    // Install DHCP client on the hosts in each VLAN
    DhcpV4ClientHelper dhcpClientHelper;
    dhcpClientHelper.Install(hostsVlan10);
    dhcpClientHelper.Install(hostsVlan20);
    dhcpClientHelper.Install(hostsVlan30);

    // Add static routes on the router
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Create traffic between hosts in different VLANs
    uint16_t port = 9;  // well-known echo port number

    // Host 1 in VLAN 10 sends traffic to host 1 in VLAN 20
    PacketSinkHelper sinkHelper ("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), port));
    ApplicationContainer sinkApp = sinkHelper.Install (hostsVlan20.Get (0));
    sinkApp.Start (Seconds (1.0));
    sinkApp.Stop (Seconds (10.0));

    Ptr<Node> appSource = hostsVlan10.Get (0);
    Ptr<Ipv4> ipv4 = appSource->GetObject<Ipv4> ();
    Ipv4InterfaceAddress iaddr = ipv4->GetAddress (1, 0);
    Ipv4Address sourceAddress = iaddr.GetLocal ();

    UdpEchoClientHelper clientHelper (routerVlan20Addr, port);
    clientHelper.SetAttribute ("MaxPackets", UintegerValue (5));
    clientHelper.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
    clientHelper.SetAttribute ("PacketSize", UintegerValue (1024));
    ApplicationContainer clientApps = clientHelper.Install (hostsVlan10.Get (0));
    clientApps.Start (Seconds (2.0));
    clientApps.Stop (Seconds (10.0));

    // Set up animation
    AnimationInterface anim("vlan-routing.xml");
    anim.SetConstantPosition(router.Get(0), 10, 10);
    anim.SetConstantPosition(sw.Get(0), 20, 10);
    anim.SetConstantPosition(hostsVlan10.Get(0), 30, 5);
    anim.SetConstantPosition(hostsVlan10.Get(1), 30, 15);
    anim.SetConstantPosition(hostsVlan20.Get(0), 40, 5);
    anim.SetConstantPosition(hostsVlan20.Get(1), 40, 15);
    anim.SetConstantPosition(hostsVlan30.Get(0), 50, 5);
    anim.SetConstantPosition(hostsVlan30.Get(1), 50, 15);

    Simulator::Stop(Seconds(11.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}