#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/dhcp-server-helper.h"
#include "ns3/vlan-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("InterVlanRoutingWithDhcp");

int main(int argc, char *argv[]) {
    bool tracing = true;
    uint16_t port = 9; // UDP port for traffic

    // Enable logging
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
    LogComponentEnable("DhcpServer", LOG_LEVEL_ALL);
    LogComponentEnable("DhcpClient", LOG_LEVEL_ALL);

    // Create nodes
    NodeContainer hosts;
    hosts.Create(3); // One host per VLAN
    Ptr<Node> switchNode = CreateObject<Node>();
    Ptr<Node> router = CreateObject<Node>();

    NodeContainer allNodes(hosts);
    allNodes.Add(switchNode);
    allNodes.Add(router);

    // Create point-to-point links
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // Connect hosts to switch
    NetDeviceContainer hostSwitchDevices[3];
    for (uint32_t i = 0; i < 3; ++i) {
        hostSwitchDevices[i] = p2p.Install(hosts.Get(i), switchNode);
    }

    // Connect switch to router
    NetDeviceContainer switchRouterDevices = p2p.Install(switchNode, router);

    // Install internet stack
    InternetStackHelper internet;
    internet.Install(allNodes);

    // Assign VLAN IDs to host interfaces on the switch
    VlanHelper vlanHelper;
    vlanHelper.SetDeviceAttribute("Mtu", UintegerValue(1500));

    // VLAN IDs and Subnets
    std::vector<uint16_t> vlanIds = {10, 20, 30};
    std::vector<Ipv4Address> baseAddresses = {
        Ipv4Address("192.168.10.0"),
        Ipv4Address("192.168.20.0"),
        Ipv4Address("192.168.30.0")
    };
    Ipv4Mask mask("255.255.255.0");

    // VLAN devices on switch
    NetDeviceContainer vlanSwitchDevices[3];
    for (uint32_t i = 0; i < 3; ++i) {
        vlanSwitchDevices[i] = vlanHelper.Install(switchNode->GetDevice(0), vlanIds[i]);
    }

    // Assign IP addresses to router sub-interfaces
    Ipv4AddressHelper ipRouterHelper;
    Ipv4InterfaceContainer routerInterfaces[3];
    for (uint32_t i = 0; i < 3; ++i) {
        ipRouterHelper.SetBase(baseAddresses[i], mask);
        routerInterfaces[i] = ipRouterHelper.Assign(vlanSwitchDevices[i]);
    }

    // Setup DHCP server for each VLAN
    DhcpServerHelper dhcpServerHelpers[3];
    ApplicationContainer dhcpServers[3];
    Ipv4Address dhcpPoolStart[3];
    Ipv4Mask dhcpPoolMask = mask;
    uint32_t dhcpPoolSize = 100;
    for (uint32_t i = 0; i < 3; ++i) {
        dhcpPoolStart[i] = baseAddresses[i];
        dhcpPoolStart[i].CombineHostMask(Ipv4Mask("/" + std::to_string(24)));
        dhcpPoolStart[i].m_value += 10; // Start from .10

        dhcpServerHelpers[i] = DhcpServerHelper(dhcpPoolStart[i], dhcpPoolMask, dhcpPoolSize);
        dhcpServerHelpers[i].SetAttribute("LeaseTimeout", TimeValue(Seconds(3600)));
        dhcpServers[i] = dhcpServerHelpers[i].Install(router);
        dhcpServers[i].Start(Seconds(1.0));
        dhcpServers[i].Stop(Seconds(20.0));
    }

    // Assign router interface as gateway in DHCP pools
    for (uint32_t i = 0; i < 3; ++i) {
        dhcpServerHelpers[i].SetGatewayAndMask(routerInterfaces[i].GetAddress(0), mask);
    }

    // Configure clients on hosts
    DhcpClientHelper dhcpClients[3];
    ApplicationContainer clientApps;
    for (uint32_t i = 0; i < 3; ++i) {
        dhcpClients[i] = DhcpClientHelper();
        clientApps.Add(dhcpClients[i].Install(hosts.Get(i)));
    }
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(20.0));

    // Enable global routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Set up traffic between hosts in different VLANs
    uint32_t packetSize = 1024;
    uint32_t maxPacketCount = 5;
    Time interPacketInterval = Seconds(1.0);

    // From VLAN 10 host to VLAN 20 host
    UdpEchoClientHelper echoClientHelper1(vlanHelper.GetServerAddress(20), port);
    echoClientHelper1.SetAttribute("MaxPackets", UintegerValue(maxPacketCount));
    echoClientHelper1.SetAttribute("Interval", TimeValue(interPacketInterval));
    echoClientHelper1.SetAttribute("PacketSize", UintegerValue(packetSize));

    ApplicationContainer clientApp1 = echoClientHelper1.Install(hosts.Get(0));
    clientApp1.Start(Seconds(5.0));
    clientApp1.Stop(Seconds(20.0));

    UdpEchoServerHelper echoServerHelper1(port);
    ApplicationContainer serverApp1 = echoServerHelper1.Install(hosts.Get(1));
    serverApp1.Start(Seconds(5.0));
    serverApp1.Stop(Seconds(20.0));

    // From VLAN 30 host to VLAN 10 host
    UdpEchoClientHelper echoClientHelper2(vlanHelper.GetServerAddress(10), port);
    echoClientHelper2.SetAttribute("MaxPackets", UintegerValue(maxPacketCount));
    echoClientHelper2.SetAttribute("Interval", TimeValue(interPacketInterval));
    echoClientHelper2.SetAttribute("PacketSize", UintegerValue(packetSize));

    ApplicationContainer clientApp2 = echoClientHelper2.Install(hosts.Get(2));
    clientApp2.Start(Seconds(7.0));
    clientApp2.Stop(Seconds(20.0));

    UdpEchoServerHelper echoServerHelper2(port);
    ApplicationContainer serverApp2 = echoServerHelper2.Install(hosts.Get(0));
    serverApp2.Start(Seconds(7.0));
    serverApp2.Stop(Seconds(20.0));

    // Pcap tracing
    if (tracing) {
        p2p.EnablePcapAll("inter-vlan-dhcp");
    }

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}