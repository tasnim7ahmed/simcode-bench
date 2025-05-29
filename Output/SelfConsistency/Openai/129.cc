// vlan-intervlan-dhcp.cc

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/applications-module.h"
#include "ns3/packet-sink.h"
#include "ns3/v4ping-helper.h"
#include "ns3/dhcp-helper.h"
#include "ns3/bridge-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("VlanInterVlanDhcpExample");

// VLAN IDs & subnet definitions
const uint16_t VLAN1_ID = 10;
const uint16_t VLAN2_ID = 20;
const uint16_t VLAN3_ID = 30;

const char *VLAN1_NET = "10.0.1.0";
const char *VLAN2_NET = "10.0.2.0";
const char *VLAN3_NET = "10.0.3.0";
const char *VLAN_SUBNET_MASK = "255.255.255.0";

int main(int argc, char *argv[])
{
    // Enable logging for DHCP
    LogComponentEnable("DhcpServer", LOG_LEVEL_INFO);
    LogComponentEnable("DhcpClient", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    CommandLine cmd;
    cmd.Parse(argc, argv);

    // Nodes: 1 Layer3 Switch (simulated using BridgeNetDevice + VLAN), 1 Router, 3 per-VLAN hosts
    NodeContainer hostsVlan1, hostsVlan2, hostsVlan3, allHosts;
    hostsVlan1.Create(1);
    hostsVlan2.Create(1);
    hostsVlan3.Create(1);
    allHosts.Add(hostsVlan1);
    allHosts.Add(hostsVlan2);
    allHosts.Add(hostsVlan3);

    Ptr<Node> l3switch = CreateObject<Node>();
    Ptr<Node> router = CreateObject<Node>();
    Ptr<Node> dhcpServer = CreateObject<Node>();

    // Create Csma channels
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));

    // Per-VLAN "links": hosts <-> switch, switch <-> router, switch <-> DHCP server
    NetDeviceContainer hostDevsVlan1, hostDevsVlan2, hostDevsVlan3;
    NetDeviceContainer switchPorts, switchRouterLink, switchDhcpLink;

    // VLAN1: host <-> switch
    NetDeviceContainer tmp = csma.Install(NodeContainer(hostsVlan1.Get(0), l3switch));
    hostDevsVlan1.Add(tmp.Get(0));
    switchPorts.Add(tmp.Get(1));

    // VLAN2: host <-> switch
    tmp = csma.Install(NodeContainer(hostsVlan2.Get(0), l3switch));
    hostDevsVlan2.Add(tmp.Get(0));
    switchPorts.Add(tmp.Get(1));

    // VLAN3: host <-> switch
    tmp = csma.Install(NodeContainer(hostsVlan3.Get(0), l3switch));
    hostDevsVlan3.Add(tmp.Get(0));
    switchPorts.Add(tmp.Get(1));

    // Switch <-> Router (Trunk)
    tmp = csma.Install(NodeContainer(l3switch, router));
    switchRouterLink.Add(tmp.Get(0)); // Switch side (for trunk)
    NetDeviceContainer routerTrunkDev; // Will use for VLAN sub-interfaces
    routerTrunkDev.Add(tmp.Get(1));    // Router side (for trunk)

    // Switch <-> DHCP Server (Trunk)
    tmp = csma.Install(NodeContainer(l3switch, dhcpServer));
    switchDhcpLink.Add(tmp.Get(0)); // Switch side (for trunk)
    NetDeviceContainer dhcpTrunkDev;
    dhcpTrunkDev.Add(tmp.Get(1));   // DHCP server side

    // Create VLAN sub-interfaces on hosts (access port semantics)
    NetDeviceContainer vlanHostDevs;
    Ptr<NetDevice> vlanDev;
    // VLAN1 Host
    vlanDev = BridgeHelper::InstallVlanDevice(hostsVlan1.Get(0), hostDevsVlan1.Get(0), VLAN1_ID);
    vlanHostDevs.Add(vlanDev);
    // VLAN2 Host
    vlanDev = BridgeHelper::InstallVlanDevice(hostsVlan2.Get(0), hostDevsVlan2.Get(0), VLAN2_ID);
    vlanHostDevs.Add(vlanDev);
    // VLAN3 Host
    vlanDev = BridgeHelper::InstallVlanDevice(hostsVlan3.Get(0), hostDevsVlan3.Get(0), VLAN3_ID);
    vlanHostDevs.Add(vlanDev);

    // On switch: configure VLAN-aware bridge, assign VLANs to access ports; trunk for router/DHCP
    BridgeHelper bridge;
    NetDeviceContainer bridgeDevs;
    // Make access ports with VLAN membership
    bridgeDevs.Add(BridgeHelper::InstallVlanDevice(l3switch, switchPorts.Get(0), VLAN1_ID));
    bridgeDevs.Add(BridgeHelper::InstallVlanDevice(l3switch, switchPorts.Get(1), VLAN2_ID));
    bridgeDevs.Add(BridgeHelper::InstallVlanDevice(l3switch, switchPorts.Get(2), VLAN3_ID));
    // Add trunk ports for router/dhcp (accept all VLANs)
    bridgeDevs.Add(switchRouterLink.Get(0));
    bridgeDevs.Add(switchDhcpLink.Get(0));
    // Install bridge (L2 switch)
    Ptr<NetDevice> bridgeDev = bridge.Install(l3switch, bridgeDevs);

    // On Router: Create 802.1Q sub-interfaces for each VLAN on the trunk port
    NetDeviceContainer routerVlanDevs;
    vlanDev = BridgeHelper::InstallVlanDevice(router, routerTrunkDev.Get(0), VLAN1_ID);
    routerVlanDevs.Add(vlanDev);
    vlanDev = BridgeHelper::InstallVlanDevice(router, routerTrunkDev.Get(0), VLAN2_ID);
    routerVlanDevs.Add(vlanDev);
    vlanDev = BridgeHelper::InstallVlanDevice(router, routerTrunkDev.Get(0), VLAN3_ID);
    routerVlanDevs.Add(vlanDev);

    // On DHCP server: Create 802.1Q sub-interfaces for each VLAN on the trunk port
    NetDeviceContainer dhcpVlanDevs;
    vlanDev = BridgeHelper::InstallVlanDevice(dhcpServer, dhcpTrunkDev.Get(0), VLAN1_ID);
    dhcpVlanDevs.Add(vlanDev);
    vlanDev = BridgeHelper::InstallVlanDevice(dhcpServer, dhcpTrunkDev.Get(0), VLAN2_ID);
    dhcpVlanDevs.Add(vlanDev);
    vlanDev = BridgeHelper::InstallVlanDevice(dhcpServer, dhcpTrunkDev.Get(0), VLAN3_ID);
    dhcpVlanDevs.Add(vlanDev);

    // Install Internet stack on all nodes
    InternetStackHelper internet;
    internet.Install(allHosts);
    internet.Install(router);
    internet.Install(dhcpServer);

    // Address assignment for router and dhcp server interfaces
    Ipv4AddressHelper address;
    // VLAN1
    address.SetBase(VLAN1_NET, VLAN_SUBNET_MASK);
    Ipv4InterfaceContainer ifaceRouterVlan1 = address.Assign(routerVlanDevs.Get(0));
    Ipv4InterfaceContainer ifaceDhcpVlan1 = address.Assign(dhcpVlanDevs.Get(0));
    address.NewNetwork();
    // VLAN2
    address.SetBase(VLAN2_NET, VLAN_SUBNET_MASK);
    Ipv4InterfaceContainer ifaceRouterVlan2 = address.Assign(routerVlanDevs.Get(1));
    Ipv4InterfaceContainer ifaceDhcpVlan2 = address.Assign(dhcpVlanDevs.Get(1));
    address.NewNetwork();
    // VLAN3
    address.SetBase(VLAN3_NET, VLAN_SUBNET_MASK);
    Ipv4InterfaceContainer ifaceRouterVlan3 = address.Assign(routerVlanDevs.Get(2));
    Ipv4InterfaceContainer ifaceDhcpVlan3 = address.Assign(dhcpVlanDevs.Get(2));

    // Enable IP forwarding on router
    Ptr<Ipv4> ipv4router = router->GetObject<Ipv4>();
    ipv4router->SetAttribute("IpForward", BooleanValue(true));

    // Configure DHCP Server Pools on each VLAN
    DhcpHelper dhcpHelper;
    dhcpHelper.SetDeviceAttribute("LeaseTime", TimeValue(Seconds(6000)));
    Ptr<Application> dhcpApp1 = dhcpHelper.InstallDhcpServer(dhcpServer, dhcpVlanDevs.Get(0),
        Ipv4Address(VLAN1_NET), Ipv4Mask(VLAN_SUBNET_MASK), // pool
        Ipv4Address("10.0.1.2"), Ipv4Address("10.0.1.100"), // range
        ifaceRouterVlan1.GetAddress(0),                     // default gateway
        ifaceDhcpVlan1.GetAddress(0));                      // dns server
    Ptr<Application> dhcpApp2 = dhcpHelper.InstallDhcpServer(dhcpServer, dhcpVlanDevs.Get(1),
        Ipv4Address(VLAN2_NET), Ipv4Mask(VLAN_SUBNET_MASK),
        Ipv4Address("10.0.2.2"), Ipv4Address("10.0.2.100"),
        ifaceRouterVlan2.GetAddress(0),
        ifaceDhcpVlan2.GetAddress(0));
    Ptr<Application> dhcpApp3 = dhcpHelper.InstallDhcpServer(dhcpServer, dhcpVlanDevs.Get(2),
        Ipv4Address(VLAN3_NET), Ipv4Mask(VLAN_SUBNET_MASK),
        Ipv4Address("10.0.3.2"), Ipv4Address("10.0.3.100"),
        ifaceRouterVlan3.GetAddress(0),
        ifaceDhcpVlan3.GetAddress(0));
    dhcpApp1->SetStartTime(Seconds(0.1));
    dhcpApp2->SetStartTime(Seconds(0.1));
    dhcpApp3->SetStartTime(Seconds(0.1));

    // Install DHCP Clients on hosts' VLAN sub-devices
    for (uint32_t i = 0; i < vlanHostDevs.GetN(); ++i)
    {
        Ptr<Application> client = dhcpHelper.InstallDhcpClient(allHosts.Get(i), vlanHostDevs.Get(i));
        client->SetStartTime(Seconds(0.2));
    }

    // Wait for DHCP assignment to complete before starting tests
    Simulator::Schedule(Seconds(3.0), []() {
        NS_LOG_INFO("Network setup complete, starting inter-VLAN traffic test...");
    });

    // Set up a UDP echo server on VLAN2 host, client on VLAN1 host, and vice versa
    // Will demonstrate inter-VLAN routing works via the router.

    // UDP Echo Server on VLAN2 host
    uint16_t echoPort = 9000;
    UdpEchoServerHelper echoServer(echoPort);
    ApplicationContainer serverApps = echoServer.Install(hostsVlan2.Get(0));
    serverApps.Start(Seconds(3.5));
    serverApps.Stop(Seconds(10.0));

    // Wait for IP assignment, then fetch host addresses and configure clients
    Simulator::Schedule(Seconds(4.0), [&]() {
        // Fetch the IP from the DHCP allocation
        Ptr<Ipv4> ipv4Host1 = hostsVlan1.Get(0)->GetObject<Ipv4>();
        Ptr<Ipv4> ipv4Host2 = hostsVlan2.Get(0)->GetObject<Ipv4>();
        // The client address is dynamic; get from interface 1 (interface 0 is loopback)
        Ipv4Address srcIp = ipv4Host1->GetAddress(1, 0).GetLocal();
        Ipv4Address dstIp = ipv4Host2->GetAddress(1, 0).GetLocal();

        NS_LOG_INFO("VLAN1 Host address: " << srcIp);
        NS_LOG_INFO("VLAN2 Host address: " << dstIp);

        // UDP Echo Client on VLAN1 host, targeting VLAN2 host
        UdpEchoClientHelper echoClient(dstIp, echoPort);
        echoClient.SetAttribute("MaxPackets", UintegerValue(2));
        echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
        echoClient.SetAttribute("PacketSize", UintegerValue(64));

        ApplicationContainer clientApps = echoClient.Install(hostsVlan1.Get(0));
        clientApps.Start(Seconds(4.5));
        clientApps.Stop(Seconds(10.0));
    });

    // Enable PCAP tracing of DHCP traffic on dhcp server trunk subdevice
    dhcpTrunkDev.Get(0)->TraceConnectWithoutContext(
        "PhyTxEnd",
        MakeCallback([](Ptr<const Packet> packet) {
            NS_LOG_INFO("DHCP server sent packet (captured for pcap)");
        })
    );
    // NS-3 pcap capture (includes DHCP packets)
    csma.EnablePcap("vlan-intervlan-dhcp-dhcp-if", dhcpTrunkDev, true, true);

    // Enable pcap tracing for all links (for debug/analysis)
    csma.EnablePcapAll("vlan-intervlan-dhcp");

    // Run simulation
    Simulator::Stop(Seconds(12.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}