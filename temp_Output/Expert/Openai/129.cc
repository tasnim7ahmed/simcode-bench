#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-routing-table-entry.h"
#include "ns3/bridge-module.h"
#include "ns3/dhcp-module.h"
#include "ns3/csma-module.h"
#include "ns3/packet-socket-helper.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // Config parameters
    uint32_t nVlanHosts = 2;

    // VLAN configuration
    std::vector<uint16_t> vlanIds = {10, 20, 30};
    std::vector<Ipv4Address> vlanSubnetBase = {Ipv4Address("10.10.1.0"), Ipv4Address("10.10.2.0"), Ipv4Address("10.10.3.0")};
    std::vector<Ipv4Mask> vlanSubnetMask = {Ipv4Mask("255.255.255.0"), Ipv4Mask("255.255.255.0"), Ipv4Mask("255.255.255.0")};
    std::vector<Ipv4Address> vlanGw = {Ipv4Address("10.10.1.1"), Ipv4Address("10.10.2.1"), Ipv4Address("10.10.3.1")};

    // Create nodes
    NodeContainer hosts[3];
    for (uint32_t i = 0; i < 3; ++i)
        hosts[i].Create(nVlanHosts);
    Ptr<Node> l3Switch = CreateObject<Node>();
    Ptr<Node> router = CreateObject<Node>();
    Ptr<Node> dhcpServer = CreateObject<Node>();

    // Enable pcap for CSMA devices
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", DataRateValue(DataRate("100Mbps")));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(1)));

    // VLAN host-L3 switch csma links and VLAN IDs
    NetDeviceContainer hostDevs[3], l3SwitchDevs[3];
    NetDeviceContainer l3SwitchRouterDevs, routerDevs;
    NetDeviceContainer dhcpSwitchDevs, dhcpDevs;

    // L3 switch-ROUTER CSMA links (one per VLAN, sub-interfaces)
    for (uint32_t vlanIdx = 0; vlanIdx < 3; ++vlanIdx)
    {
        // Host to switch
        for (uint32_t h = 0; h < nVlanHosts; ++h)
        {
            NodeContainer nc;
            nc.Add(hosts[vlanIdx].Get(h));
            nc.Add(l3Switch);
            NetDeviceContainer d = csma.Install(nc);
            d.Get(0)->SetAttribute("PacketFilters", UintegerValue(0)); // Host side
            Ptr<CsmaNetDevice> swDev = DynamicCast<CsmaNetDevice>(d.Get(1));
            swDev->SetAttribute("PacketFilters", UintegerValue(0));
            // Configure VLAN on the L3 switch port
            Ptr<Dot1qNetDevice> vlanDev = CreateObject<Dot1qNetDevice>();
            vlanDev->SetVlanId(vlanIds[vlanIdx]);
            vlanDev->SetIfIndex(swDev->GetIfIndex());
            swDev->AggregateObject(vlanDev);
            hostDevs[vlanIdx].Add(d.Get(0));
            l3SwitchDevs[vlanIdx].Add(d.Get(1));
        }
        // DHCP server to switch for each VLAN
        NodeContainer dhcpNc;
        dhcpNc.Add(dhcpServer);
        dhcpNc.Add(l3Switch);
        NetDeviceContainer d = csma.Install(dhcpNc);
        dhcpDevs.Add(d.Get(0));
        // Attach VLAN tag for each switch port facing DHCP server
        Ptr<CsmaNetDevice> swDev = DynamicCast<CsmaNetDevice>(d.Get(1));
        Ptr<Dot1qNetDevice> vlanDev = CreateObject<Dot1qNetDevice>();
        vlanDev->SetVlanId(vlanIds[vlanIdx]);
        vlanDev->SetIfIndex(swDev->GetIfIndex());
        swDev->AggregateObject(vlanDev);
        dhcpSwitchDevs.Add(d.Get(1));
    }

    // L3 switch to Router
    NodeContainer switchRouterNc;
    switchRouterNc.Add(l3Switch);
    switchRouterNc.Add(router);
    NetDeviceContainer d = csma.Install(switchRouterNc);
    l3SwitchRouterDevs.Add(d.Get(0));
    routerDevs.Add(d.Get(1));

    // Internet stack
    InternetStackHelper internet;
    for (uint32_t i=0;i<3;++i) internet.Install(hosts[i]);
    internet.Install(l3Switch);
    internet.Install(router);
    internet.Install(dhcpServer);

    // L3 Switch: assign VLAN sub-interfaces
    Ipv4AddressHelper address;
    Ipv4InterfaceContainer switchIfs[3], routerIfs[3], dhcpIfs[3], hostIfs[3];
    for (uint32_t vlanIdx = 0; vlanIdx < 3; ++vlanIdx)
    {
        // Router VLAN subinterface
        Ptr<CsmaNetDevice> rDev = DynamicCast<CsmaNetDevice>(routerDevs.Get(0));
        Ptr<Dot1qNetDevice> routerVlanDev = CreateObject<Dot1qNetDevice>();
        routerVlanDev->SetVlanId(vlanIds[vlanIdx]);
        routerVlanDev->SetIfIndex(rDev->GetIfIndex());
        rDev->AggregateObject(routerVlanDev);

        // Assign IP to router VLAN subinterface (default gateway)
        address.SetBase(vlanSubnetBase[vlanIdx], vlanSubnetMask[vlanIdx]);
        NetDeviceContainer routerVlanDevCont;
        routerVlanDevCont.Add(routerDevs.Get(0));
        routerIfs[vlanIdx] = address.Assign(routerVlanDevCont);

        // Assign IP to DHCP server VLAN interface
        Ptr<CsmaNetDevice> dDev = DynamicCast<CsmaNetDevice>(dhcpDevs.Get(vlanIdx));
        Ptr<Dot1qNetDevice> dhcpVlanDev = CreateObject<Dot1qNetDevice>();
        dhcpVlanDev->SetVlanId(vlanIds[vlanIdx]);
        dhcpVlanDev->SetIfIndex(dDev->GetIfIndex());
        dDev->AggregateObject(dhcpVlanDev);

        NetDeviceContainer dhcpVlanDevCont;
        dhcpVlanDevCont.Add(dhcpDevs.Get(vlanIdx));
        dhcpIfs[vlanIdx] = address.Assign(dhcpVlanDevCont);

        // Host devices: DHCP will configure them
        for (uint32_t h = 0; h < nVlanHosts; ++h)
        {
            NetDeviceContainer hostDevCont;
            hostDevCont.Add(hostDevs[vlanIdx].Get(h));
        }
    }

    // Configure DHCP servers, one per VLAN
    DhcpHelper dhcpHelper;
    ApplicationContainer dhcpApps;
    for (uint32_t vlanIdx = 0; vlanIdx < 3; ++vlanIdx)
    {
        // Pool: skip the router's address (.1), assign to .100-.200
        Ipv4Address poolStart = Ipv4Address(vlanSubnetBase[vlanIdx].Get() + 100);
        Ipv4Address poolEnd = Ipv4Address(vlanSubnetBase[vlanIdx].Get() + 200);
        uint32_t netIntIdx = dhcpIfs[vlanIdx].GetInterfaceIndex(0);
        DhcpHelper::DhcpServerConfig serverConfig;
        serverConfig.interface = dhcpServer->GetObject<Ipv4>()->GetNetDevice(netIntIdx);
        serverConfig.leaseStart = poolStart;
        serverConfig.leaseEnd = poolEnd;
        serverConfig.leaseTime = Seconds(3600);
        serverConfig.router = vlanGw[vlanIdx];
        serverConfig.subnetMask = vlanSubnetMask[vlanIdx];
        serverConfig.broadcast = Ipv4Address(vlanSubnetBase[vlanIdx].CombineMask(vlanSubnetMask[vlanIdx]) | ~vlanSubnetMask[vlanIdx]);
        ApplicationContainer serverApp = dhcpHelper.InstallDhcpServer(dhcpServer, serverConfig);
        dhcpApps.Add(serverApp);
    }

    // Configure DHCP clients on hosts
    for (uint32_t vlanIdx = 0; vlanIdx < 3; ++vlanIdx)
    {
        for (uint32_t h = 0; h < nVlanHosts; ++h)
        {
            Ptr<Node> host = hosts[vlanIdx].Get(h);
            DhcpHelper::DhcpClientConfig clientConfig;
            clientConfig.interface = host->GetDevice(0);
            ApplicationContainer clientApp = dhcpHelper.InstallDhcpClient(host, clientConfig);
        }
    }

    // Enable pcap on dhcp server interfaces
    // Trace only DHCP (UDP 67/68)
    for (uint32_t i=0;i<3;++i)
    {
        csma.EnablePcap("dhcp-vlan", dhcpDevs.Get(i), true, true);
    }

    // Generate cross-VLAN traffic tests (e.g., ICMP Echo request from VLAN 10 node to VLAN 20 node)
    uint16_t port = 9;
    for (uint32_t srcVlan = 0; srcVlan < 3; ++srcVlan)
    {
        uint32_t dstVlan = (srcVlan + 1) % 3;
        Ptr<Node> srcHost = hosts[srcVlan].Get(0);
        Ptr<Node> dstHost = hosts[dstVlan].Get(0);

        // UDP packet from srcHost to dstHost (address assigned by DHCP, so schedule after DHCP)
        UdpEchoServerHelper echoServer(port);
        ApplicationContainer serverApps = echoServer.Install(dstHost);
        serverApps.Start(Seconds(5.0));
        serverApps.Stop(Seconds(20.0));
        UdpEchoClientHelper echoClient(Ipv4Address::GetAny(), port);
        echoClient.SetAttribute("MaxPackets", UintegerValue(4));
        echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
        echoClient.SetAttribute("PacketSize", UintegerValue(64));
        ApplicationContainer clientApps = echoClient.Install(srcHost);
        clientApps.Start (Seconds(6.0));
        clientApps.Stop (Seconds(20.0));
    }

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    Simulator::Stop(Seconds(30.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}