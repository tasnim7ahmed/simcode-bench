#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/vlan-module.h"
#include "ns3/dhcp-module.h"

NS_LOG_COMPONENT_DEFINE("InterVlanRoutingDhcp");

int main(int argc, char *argv[])
{
    LogComponentEnable("InterVlanRoutingDhcp", LOG_LEVEL_INFO);
    LogComponentEnable("DhcpClient", LOG_LEVEL_INFO);
    LogComponentEnable("DhcpServer", LOG_LEVEL_INFO);
    LogComponentEnable("DhcpRelay", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    // 1. Create nodes
    NodeContainer allNodes;
    allNodes.Create(5); // L3 Router, hostVlan10, hostVlan20, hostVlan30, DhcpServer

    Ptr<Node> l3RouterNode = allNodes.Get(0);
    Ptr<Node> hostVlan10 = allNodes.Get(1);
    Ptr<Node> hostVlan20 = allNodes.Get(2);
    Ptr<Node> hostVlan30 = allNodes.Get(3);
    Ptr<Node> dhcpServerNode = allNodes.Get(4);

    // 2. Install Internet Stack on all nodes
    InternetStackHelper stack;
    stack.Install(allNodes);

    // 3. Create CSMA channel (representing the switch backplane)
    CsmaHelper csmaHelper;
    csmaHelper.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csmaHelper.SetChannelAttribute("Delay", StringValue("10ms"));

    NetDeviceContainer csmaDevices;
    csmaDevices = csmaHelper.Install(allNodes);

    // Get the CSMA NetDevice of the router node
    Ptr<NetDevice> routerCsmaDevice = csmaDevices.Get(0);

    // 4. Configure VLANs on the L3 Router
    // VLAN 10 (10.0.10.0/24)
    VlanHelper vlan10Helper;
    Ptr<VlanNetDevice> vlan10RouterDevice = vlan10Helper.Create(routerCsmaDevice, 10);
    l3RouterNode->AddDevice(vlan10RouterDevice);
    Ipv4AddressHelper ipv4Vlan10;
    ipv4Vlan10.SetBase("10.0.10.0", "255.255.255.0");
    Ipv4InterfaceContainer routerVlan10If = ipv4Vlan10.Assign(NetDeviceContainer(vlan10RouterDevice));
    // Set router's VLAN 10 interface IP: 10.0.10.1 (gateway for VLAN 10)
    routerVlan10If.SetAddress(0, Ipv4Address("10.0.10.1"));

    // VLAN 20 (10.0.20.0/24)
    VlanHelper vlan20Helper;
    Ptr<VlanNetDevice> vlan20RouterDevice = vlan20Helper.Create(routerCsmaDevice, 20);
    l3RouterNode->AddDevice(vlan20RouterDevice);
    Ipv4AddressHelper ipv4Vlan20;
    ipv4Vlan20.SetBase("10.0.20.0", "255.255.255.0");
    Ipv4InterfaceContainer routerVlan20If = ipv4Vlan20.Assign(NetDeviceContainer(vlan20RouterDevice));
    // Set router's VLAN 20 interface IP: 10.0.20.1 (gateway for VLAN 20)
    routerVlan20If.SetAddress(0, Ipv4Address("10.0.20.1"));

    // VLAN 30 (10.0.30.0/24)
    VlanHelper vlan30Helper;
    Ptr<VlanNetDevice> vlan30RouterDevice = vlan30Helper.Create(routerCsmaDevice, 30);
    l3RouterNode->AddDevice(vlan30RouterDevice);
    Ipv4AddressHelper ipv4Vlan30;
    ipv4Vlan30.SetBase("10.0.30.0", "255.255.255.0");
    Ipv4InterfaceContainer routerVlan30If = ipv4Vlan30.Assign(NetDeviceContainer(vlan30RouterDevice));
    // Set router's VLAN 30 interface IP: 10.0.30.1 (gateway for VLAN 30)
    routerVlan30If.SetAddress(0, Ipv4Address("10.0.30.1"));

    // Enable IP forwarding on the L3 router
    Ptr<Ipv4> ipv4Router = l3RouterNode->GetObject<Ipv4>();
    ipv4Router->SetForwarding(0, true);

    // 5. Configure DHCP Server
    // Assign static IP to DHCP server (part of VLAN 10 subnet)
    Ipv4AddressHelper ipv4DhcpServer;
    ipv4DhcpServer.SetBase("10.0.10.0", "255.255.255.0");
    Ipv4InterfaceContainer dhcpServerIf = ipv4DhcpServer.Assign(NetDeviceContainer(csmaDevices.Get(4)));
    dhcpServerIf.SetAddress(0, Ipv4Address("10.0.10.2")); // Static IP for DHCP server

    DhcpServerHelper dhcpServerHelper;
    // Add DHCP pools for each VLAN, using the router's VLAN interface IPs as gateways
    dhcpServerHelper.AddPool(routerVlan10If.GetAddress(0), ipv4Vlan10.GetMask()); // VLAN 10 pool
    dhcpServerHelper.AddPool(routerVlan20If.GetAddress(0), ipv4Vlan20.GetMask()); // VLAN 20 pool
    dhcpServerHelper.AddPool(routerVlan30If.GetAddress(0), ipv4Vlan30.GetMask()); // VLAN 30 pool

    ApplicationContainer dhcpServerApps = dhcpServerHelper.Install(dhcpServerNode);
    dhcpServerApps.Start(Seconds(0.0));
    dhcpServerApps.Stop(Seconds(60.0));

    // 6. Configure DHCP Relay on the L3 Router
    // This is for clients in VLANs 20 and 30 to reach the DHCP server in VLAN 10.
    DhcpRelayHelper dhcpRelayHelper;
    // Relay from VLAN 20 client subnet to DHCP server through VLAN 10
    dhcpRelayHelper.AddRelay(vlan20RouterDevice, routerVlan10If.GetAddress(0), routerVlan20If.GetAddress(0), dhcpServerIf.GetAddress(0));
    // Relay from VLAN 30 client subnet to DHCP server through VLAN 10
    dhcpRelayHelper.AddRelay(vlan30RouterDevice, routerVlan10If.GetAddress(0), routerVlan30If.GetAddress(0), dhcpServerIf.GetAddress(0));

    ApplicationContainer dhcpRelayApps = dhcpRelayHelper.Install(l3RouterNode);
    dhcpRelayApps.Start(Seconds(0.0));
    dhcpRelayApps.Stop(Seconds(60.0));

    // 7. Configure DHCP Clients
    DhcpClientHelper dhcpClientHelper;

    // Host in VLAN 10
    dhcpClientHelper.SetVlanTag(10); // Tag packets with VLAN ID 10
    ApplicationContainer dhcpClient10Apps = dhcpClientHelper.Install(hostVlan10);
    dhcpClient10Apps.Start(Seconds(0.0));
    dhcpClient10Apps.Stop(Seconds(60.0));

    // Host in VLAN 20
    dhcpClientHelper.SetVlanTag(20); // Tag packets with VLAN ID 20
    ApplicationContainer dhcpClient20Apps = dhcpClientHelper.Install(hostVlan20);
    dhcpClient20Apps.Start(Seconds(0.0));
    dhcpClient20Apps.Stop(Seconds(60.0));

    // Host in VLAN 30
    dhcpClientHelper.SetVlanTag(30); // Tag packets with VLAN ID 30
    ApplicationContainer dhcpClient30Apps = dhcpClientHelper.Install(hostVlan30);
    dhcpClient30Apps.Start(Seconds(0.0));
    dhcpClient30Apps.Stop(Seconds(60.0));

    // 8. Configure Traffic (UDP Echo) between different VLANs
    // Sender: hostVlan10 (VLAN 10)
    // Receiver: hostVlan20 (VLAN 20)

    // Install UDP Echo Server on hostVlan20
    UdpEchoServerHelper echoServer(9); // Port 9
    ApplicationContainer serverApps = echoServer.Install(hostVlan20);
    serverApps.Start(Seconds(5.1)); // Start after DHCP has likely assigned IPs
    serverApps.Stop(Seconds(60.0));

    // Install UDP Echo Client on hostVlan10
    // We schedule this to run after DHCP has had a chance to assign IPs.
    Simulator::Schedule(Seconds(5.5), [hostVlan10, hostVlan20]() {
        Ptr<Ipv4> ipv4Host20 = hostVlan20->GetObject<Ipv4>();
        Ipv4Address host20Ip = ipv4Host20->GetAddress(1, 0).GetLocal(); // Get first assigned unicast address

        if (host20Ip == Ipv4Address::GetZero()) {
            NS_LOG_ERROR("hostVlan20 has no IP address assigned yet! Cannot set up UDP echo client.");
            return;
        }

        UdpEchoClientHelper echoClient(host20Ip, 9);
        echoClient.SetAttribute("MaxPackets", UintegerValue(3));
        echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
        echoClient.SetAttribute("PacketSize", UintegerValue(1024));
        ApplicationContainer clientApps = echoClient.Install(hostVlan10);
        clientApps.Start(Seconds(6.0));
        clientApps.Stop(Seconds(60.0));
    });

    // 9. Enable Pcap Tracing for DHCP and general traffic
    // Tracing on CSMA devices where DHCP packets travel
    csmaHelper.EnablePcap("inter-vlan-dhcp", csmaDevices.Get(1), true, true); // hostVlan10
    csmaHelper.EnablePcap("inter-vlan-dhcp", csmaDevices.Get(2), true, true); // hostVlan20
    csmaHelper.EnablePcap("inter-vlan-dhcp", csmaDevices.Get(3), true, true); // hostVlan30
    csmaHelper.EnablePcap("inter-vlan-dhcp", csmaDevices.Get(0), true, true); // L3 Router (sees all VLAN traffic)
    csmaHelper.EnablePcap("inter-vlan-dhcp", csmaDevices.Get(4), true, true); // DHCP Server

    // 10. Run simulation
    Simulator::Stop(Seconds(65.0)); // Run long enough for DHCP and UDP echo
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}