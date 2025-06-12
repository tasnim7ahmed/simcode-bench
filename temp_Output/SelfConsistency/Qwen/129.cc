#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/csma-module.h"
#include "ns3/dhcp-server-helper.h"
#include "ns3/vlan-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("VlanInterRoutingDhcpExample");

int main(int argc, char *argv[]) {
    bool tracing = true;

    CommandLine cmd(__FILE__);
    cmd.AddValue("tracing", "Enable pcap tracing", tracing);
    cmd.Parse(argc, argv);

    // Create nodes
    NodeContainer hosts;
    hosts.Create(6);  // Two hosts per VLAN (3 VLANs)

    NodeContainer switchNode;
    switchNode.Create(1);

    NodeContainer router;
    router.Create(1);

    // Point-to-point links between router and switch
    PointToPointHelper p2pSwitchRouter;
    p2pSwitchRouter.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
    p2pSwitchRouter.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer switchRouterDevices = p2pSwitchRouter.Install(switchNode.Get(0), router.Get(0));

    // CSMA links for each VLAN on the switch
    CsmaHelper csmaVlan1, csmaVlan2, csmaVlan3;
    csmaVlan1.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csmaVlan2.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csmaVlan3.SetChannelAttribute("DataRate", StringValue("100Mbps"));

    csmaVlan1.SetChannelAttribute("Delay", StringValue("2ms"));
    csmaVlan2.SetChannelAttribute("Delay", StringValue("2ms"));
    csmaVlan3.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer vlan1Devs, vlan2Devs, vlan3Devs;

    // Install CSMA devices on VLANs
    NodeContainer vlan1Hosts(hosts.Get(0), hosts.Get(1));
    NodeContainer vlan2Hosts(hosts.Get(2), hosts.Get(3));
    NodeContainer vlan3Hosts(hosts.Get(4), hosts.Get(5));

    vlan1Devs = csmaVlan1.Install(vlan1Hosts);
    vlan2Devs = csmaVlan2.Install(vlan2Hosts);
    vlan3Devs = csmaVlan3.Install(vlan3Hosts);

    // VLAN helper to assign VLAN IDs
    VlanHelper vlanHelper;
    vlanHelper.AssignVlanIds(vlan1Devs, 10);
    vlanHelper.AssignVlanIds(vlan2Devs, 20);
    vlanHelper.AssignVlanIds(vlan3Devs, 30);

    // Bridge the VLANs to the switch node
    NetDeviceContainer switchPorts;
    switchPorts.Add(vlan1Devs);
    switchPorts.Add(vlan2Devs);
    switchPorts.Add(vlan3Devs);
    switchPorts.Add(switchRouterDevices.Get(0));  // Port connected to router

    // BridgeHelper to simulate L2 switch behavior
    BridgeHelper bridge;
    bridge.Install(switchNode.Get(0), switchPorts);

    // Install internet stack
    InternetStackHelper stack;
    stack.InstallAll();

    // Assign IP addresses via DHCP
    DhcpServerHelper dhcpServer(router->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal());

    ApplicationContainer dhcpApps = dhcpServer.Install(vlan1Hosts.Get(0));
    dhcpApps.Add(dhcpServer.Install(vlan2Hosts.Get(0)));
    dhcpApps.Add(dhcpServer.Install(vlan3Hosts.Get(0)));
    dhcpApps.Start(Seconds(1.0));
    dhcpApps.Stop(Seconds(10.0));

    // Configure router interfaces
    Ipv4AddressHelper ipv4RouterSwitch;
    ipv4RouterSwitch.SetBase("192.168.0.0", "255.255.255.0");
    Ipv4InterfaceContainer routerSwitchIfc = ipv4RouterSwitch.Assign(switchRouterDevices.Get(1));

    // Router subnets for VLANs
    Ipv4AddressHelper ipv4SubnetVlan1;
    ipv4SubnetVlan1.SetBase("10.1.1.0", "255.255.255.0");
    ipv4SubnetVlan1.Assign(vlan1Devs);

    Ipv4AddressHelper ipv4SubnetVlan2;
    ipv4SubnetVlan2.SetBase("10.1.2.0", "255.255.255.0");
    ipv4SubnetVlan2.Assign(vlan2Devs);

    Ipv4AddressHelper ipv4SubnetVlan3;
    ipv4SubnetVlan3.SetBase("10.1.3.0", "255.255.255.0");
    ipv4SubnetVlan3.Assign(vlan3Devs);

    // Set default routes on hosts
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Traffic: Ping from host in VLAN 10 to host in VLAN 20
    V4PingHelper ping(vlan2Hosts.Get(1)->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal().PrintIp());
    ping.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    ping.SetAttribute("Size", UintegerValue(1024));
    ApplicationContainer apps = ping.Install(vlan1Hosts.Get(0));
    apps.Start(Seconds(2.0));
    apps.Stop(Seconds(10.0));

    // Pcap tracing
    if (tracing) {
        p2pSwitchRouter.EnablePcapAll("vlan-inter-routing-dhcp", false);
        csmaVlan1.EnablePcap("vlan-inter-routing-dhcp-vlan1", vlan1Devs, false);
        csmaVlan2.EnablePcap("vlan-inter-routing-dhcp-vlan2", vlan2Devs, false);
        csmaVlan3.EnablePcap("vlan-inter-routing-dhcp-vlan3", vlan3Devs, false);
    }

    NS_LOG_INFO("Run Simulation.");
    Simulator::Run();
    Simulator::Destroy();
    NS_LOG_INFO("Done.");

    return 0;
}