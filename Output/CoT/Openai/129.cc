#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/dhcp-server-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/packet-socket-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("VlanInterVlanDhcpExample");

int main (int argc, char *argv[])
{
    CommandLine cmd (__FILE__);
    cmd.Parse (argc, argv);

    uint32_t nVlanHosts = 2; // hosts per VLAN
    uint32_t nVlans = 3;
    double simTime = 15.0;

    // Create nodes
    NodeContainer vlan1Hosts, vlan2Hosts, vlan3Hosts;
    vlan1Hosts.Create (nVlanHosts);
    vlan2Hosts.Create (nVlanHosts);
    vlan3Hosts.Create (nVlanHosts);
    NodeContainer switchNode;
    switchNode.Create (1);
    NodeContainer routerNode;
    routerNode.Create (1);
    NodeContainer dhcpNode;
    dhcpNode.Create (1);

    // Install Internet stack
    InternetStackHelper internet;
    internet.Install (routerNode);
    internet.Install (vlan1Hosts);
    internet.Install (vlan2Hosts);
    internet.Install (vlan3Hosts);

    // DHCP server node does not need full TCP/IP stack,
    // but for DHCP Server App we need at least UDP/IPv4 stack
    InternetStackHelper dhcpInternet;
    dhcpInternet.SetIpv4StackInstall(true);
    dhcpInternet.Install (dhcpNode);

    // Create the CSMA segments (one per VLAN)
    CsmaHelper csmaVlan[3];
    csmaVlan[0].SetChannelAttribute ("DataRate", StringValue ("100Mbps"));
    csmaVlan[0].SetChannelAttribute ("Delay", TimeValue (NanoSeconds (6560)));
    csmaVlan[1].SetChannelAttribute ("DataRate", StringValue ("100Mbps"));
    csmaVlan[1].SetChannelAttribute ("Delay", TimeValue (NanoSeconds (6560)));
    csmaVlan[2].SetChannelAttribute ("DataRate", StringValue ("100Mbps"));
    csmaVlan[2].SetChannelAttribute ("Delay", TimeValue (NanoSeconds (6560)));

    // VLANs [0] - 10.1.1.0/24, [1] - 10.2.2.0/24, [2] - 10.3.3.0/24
    Ipv4Address subnetBase[3] = {Ipv4Address("10.1.1.0"),
                                 Ipv4Address("10.2.2.0"),
                                 Ipv4Address("10.3.3.0")};
    Ipv4Mask subnetMask ("255.255.255.0");

    // We’ll need containers for all NetDevices and interfaces
    NetDeviceContainer vlan1Devices, vlan2Devices, vlan3Devices;
    NetDeviceContainer vlan1SwitchRouter, vlan2SwitchRouter, vlan3SwitchRouter;
    NetDeviceContainer vlan1SwitchDhcp, vlan2SwitchDhcp, vlan3SwitchDhcp;

    // --- Connect hosts, switch, router for each VLAN ---
    // VLAN 1
    NodeContainer vlan1Nodes;
    for (uint32_t i = 0; i < nVlanHosts; ++i) { vlan1Nodes.Add (vlan1Hosts.Get(i)); }
    vlan1Nodes.Add (switchNode.Get(0));
    NetDeviceContainer vlan1 = csmaVlan[0].Install (vlan1Nodes);
    for (uint32_t i = 0; i < nVlanHosts; ++i) { vlan1Devices.Add (vlan1.Get(i)); }
    NetDeviceContainer vlan1Switch; vlan1Switch.Add (vlan1.Get(nVlanHosts)); // switch's device for vlan1

    // VLAN 2
    NodeContainer vlan2Nodes;
    for (uint32_t i = 0; i < nVlanHosts; ++i) { vlan2Nodes.Add (vlan2Hosts.Get(i)); }
    vlan2Nodes.Add (switchNode.Get(0));
    NetDeviceContainer vlan2 = csmaVlan[1].Install (vlan2Nodes);
    for (uint32_t i = 0; i < nVlanHosts; ++i) { vlan2Devices.Add (vlan2.Get(i)); }
    NetDeviceContainer vlan2Switch; vlan2Switch.Add (vlan2.Get(nVlanHosts)); // switch's device for vlan2

    // VLAN 3
    NodeContainer vlan3Nodes;
    for (uint32_t i = 0; i < nVlanHosts; ++i) { vlan3Nodes.Add (vlan3Hosts.Get(i)); }
    vlan3Nodes.Add (switchNode.Get(0));
    NetDeviceContainer vlan3 = csmaVlan[2].Install (vlan3Nodes);
    for (uint32_t i = 0; i < nVlanHosts; ++i) { vlan3Devices.Add (vlan3.Get(i)); }
    NetDeviceContainer vlan3Switch; vlan3Switch.Add (vlan3.Get(nVlanHosts)); // switch's device for vlan3

    // --- Switch-Router connections via SUB-Interfaces on Switch (simulate router-on-a-stick) ---
    // Each VLAN’s switch "port" has a connection to the router.
    // We use bridge/vlan NetDevice and VLAN tags to simulate a L3 switch

    // Create point-to-point link between switch and router (for trunk with sub-interfaces)
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute ("DataRate", StringValue ("100Mbps"));
    p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));
    NetDeviceContainer switchRouterDevices = p2p.Install (NodeContainer (switchNode.Get(0), routerNode.Get(0)));

    // Attach VLAN interfaces (sub-interfaces) to router: use VirtualNetDevice+VlanTag for each
    Ptr<Node> router = routerNode.Get(0);
    Ptr<Node> sw = switchNode.Get(0);

    // Install bridge/VLAN on the switchNode, so it's a virtual L3 switch (with trunk)
    BridgeHelper bridge;
    NetDeviceContainer bridgePorts;
    bridgePorts.Add(vlan1Switch.Get(0));
    bridgePorts.Add(vlan2Switch.Get(0));
    bridgePorts.Add(vlan3Switch.Get(0));
    bridgePorts.Add(switchRouterDevices.Get(0)); // trunk port to router
    NetDeviceContainer bridgeDev = bridge.Install (sw, bridgePorts);

    // --- DHCP Server connections: connect DHCP server to switch VLANs ---
    // For each VLAN, connect DHCP server to CSMA
    NetDeviceContainer dhcpVlanDevs[3];
    for (uint32_t v=0; v<3; ++v)
    {
        NodeContainer nodes; nodes.Add(dhcpNode.Get(0)); nodes.Add(switchNode.Get(0));
        NetDeviceContainer netDevs = csmaVlan[v].Install(nodes);
        dhcpVlanDevs[v].Add(netDevs.Get(0)); // DHCP side
        // Only use DHCP server side, switch port already bridged above
    }

    // Assign VLAN tags to DHCP server devices
    for (uint32_t v=0; v<3; ++v)
    {
        Ptr<NetDevice> dev = dhcpVlanDevs[v].Get(0);
        Ptr<CsmaNetDevice> csmaDev = dev->GetObject<CsmaNetDevice>();
        csmaDev->SetVlan (v+1); // VLAN 1,2,3
    }

    // VLAN Trunk: The switch-router link is trunk with router doing sub-interfaces per VLAN.
    // Use the VLAN functionality in ns-3 as simulation to create sub-interfaces on the router node.

    Ipv4InterfaceContainer routerIfs[3];
    Ipv4AddressHelper iphelp;
    for (uint32_t v=0; v<3; ++v)
    {
        // Create VirtualNetDevice as VLAN subinterface on router
        Ptr<VirtualNetDevice> virt = CreateObject<VirtualNetDevice> ();
        router->AddDevice (virt);
        virt->SetAddress (Mac48Address::Allocate ());
        virt->SetIfIndex (router->GetNDevices()-1);
        RouterInterfaceHelper::ConfigureVlanSubinterface (router, switchRouterDevices.Get(1), v+1, virt);

        Ipv4InterfaceContainer ifc;
        iphelp.SetBase (subnetBase[v], subnetMask);
        ifc = iphelp.Assign (NetDeviceContainer (virt));
        routerIfs[v].Add (ifc.Get(0));
    }

    // Assign VLAN tags to hosts' CSMA devices
    for (uint32_t i=0; i<nVlanHosts; ++i)
    {
        vlan1Devices.Get(i)->GetObject<CsmaNetDevice>()->SetVlan(1);
    }
    for (uint32_t i=0; i<nVlanHosts; ++i)
    {
        vlan2Devices.Get(i)->GetObject<CsmaNetDevice>()->SetVlan(2);
    }
    for (uint32_t i=0; i<nVlanHosts; ++i)
    {
        vlan3Devices.Get(i)->GetObject<CsmaNetDevice>()->SetVlan(3);
    }

    // --- Setup DHCP server per VLAN ---
    // Each subnet range reserves .2 - .99 for clients, .1 for router
    std::vector<uint32_t> vlanIdVec = {1,2,3};
    DhcpServerHelper dhcpHelper;
    dhcpHelper.Set ("LeasedIpFirst", Ipv4AddressValue ("10.1.1.2"));
    dhcpHelper.Set ("LeasedIpLast", Ipv4AddressValue ("10.1.1.99"));
    dhcpHelper.Set ("Netmask", Ipv4MaskValue ("255.255.255.0"));
    dhcpHelper.Set ("Router", Ipv4AddressValue ("10.1.1.1"));
    ApplicationContainer dhcpApps;
    dhcpApps.Add (dhcpHelper.Install (dhcpNode, dhcpVlanDevs[0]));

    dhcpHelper.Set ("LeasedIpFirst", Ipv4AddressValue ("10.2.2.2"));
    dhcpHelper.Set ("LeasedIpLast", Ipv4AddressValue ("10.2.2.99"));
    dhcpHelper.Set ("Netmask", Ipv4MaskValue ("255.255.255.0"));
    dhcpHelper.Set ("Router", Ipv4AddressValue ("10.2.2.1"));
    dhcpApps.Add (dhcpHelper.Install (dhcpNode, dhcpVlanDevs[1]));

    dhcpHelper.Set ("LeasedIpFirst", Ipv4AddressValue ("10.3.3.2"));
    dhcpHelper.Set ("LeasedIpLast", Ipv4AddressValue ("10.3.3.99"));
    dhcpHelper.Set ("Netmask", Ipv4MaskValue ("255.255.255.0"));
    dhcpHelper.Set ("Router", Ipv4AddressValue ("10.3.3.1"));
    dhcpApps.Add (dhcpHelper.Install (dhcpNode, dhcpVlanDevs[2]));

    dhcpApps.Start (Seconds (0.1));
    dhcpApps.Stop (Seconds (simTime));

    // --- Enable DHCP client for all hosts
    DhcpClientHelper dhcpClient;
    for (uint32_t i = 0; i < nVlanHosts; ++i)
    {
        dhcpClient.Install (vlan1Hosts.Get(i), vlan1Devices.Get(i));
        dhcpClient.Install (vlan2Hosts.Get(i), vlan2Devices.Get(i));
        dhcpClient.Install (vlan3Hosts.Get(i), vlan3Devices.Get(i));
    }

    // --- Enable Routing on router ---
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // --- Install traffic generator between different VLAN hosts ---
    // Pick vlan1 host 0 and vlan2 host 0; vlan2 host 1 and vlan3 host 1
    uint16_t port = 5000;
    OnOffHelper onoff ("ns3::UdpSocketFactory", Address ());
    onoff.SetAttribute ("PacketSize", UintegerValue (512));
    onoff.SetAttribute ("DataRate", DataRateValue (DataRate ("1Mbps")));
    Address sinkLocalAddress (InetSocketAddress (Ipv4Address::GetAny (), port));
    PacketSinkHelper sinkHelper ("ns3::UdpSocketFactory", sinkLocalAddress);

    ApplicationContainer sinkApps, srcApps;

    // For traffic to work, we need to wait for DHCP and then assign destination dynamically.
    // Use function scheduled after DHCP lease obtained.

    Ptr<Node> h1 = vlan1Hosts.Get(0);
    Ptr<Node> h2 = vlan2Hosts.Get(0);
    Ptr<Node> h3 = vlan3Hosts.Get(1);

    Ptr<Ipv4> ipv4h2 = h2->GetObject<Ipv4>();
    Ptr<Ipv4> ipv4h3 = h3->GetObject<Ipv4>();

    // Install sinks early (will bind to ANY)
    sinkApps.Add (sinkHelper.Install (h2));
    sinkApps.Add (sinkHelper.Install (h3));
    sinkApps.Start (Seconds (1.5));
    sinkApps.Stop (Seconds (simTime));

    // Schedule OnOff traffic after DHCP is expected to finish
    Simulator::Schedule (Seconds (2.5), [&](){
        Ptr<Ipv4> ipv4_ = h2->GetObject<Ipv4>();
        Ipv4Address dst = ipv4_->GetAddress (1,0).GetLocal ();
        onoff.SetAttribute ("Remote", AddressValue (InetSocketAddress (dst, port)));
        srcApps.Add (onoff.Install (h1));
        srcApps.Get(0)->SetStartTime (Seconds (3.0));
        srcApps.Get(0)->SetStopTime (Seconds (simTime - 1));
    });
    Simulator::Schedule (Seconds (3.5), [&](){
        Ptr<Ipv4> ipv42_ = h3->GetObject<Ipv4>();
        Ipv4Address dst2 = ipv42_->GetAddress (1,0).GetLocal ();
        onoff.SetAttribute ("Remote", AddressValue (InetSocketAddress (dst2, port)));
        srcApps.Add (onoff.Install (vlan2Hosts.Get(1)));
        srcApps.Get(1)->SetStartTime (Seconds (4.0));
        srcApps.Get(1)->SetStopTime (Seconds (simTime - 1));
    });

    // --- Enable pcap tracing for DHCP traffic ---
    for (uint32_t v = 0; v < 3; ++v)
    {
        dhcpVlanDevs[v].Get(0)->TraceConnectWithoutContext ("PromiscSniffer", MakeCallback (
            [](Ptr<const Packet> pkt, uint16_t proto, const Address &from, const Address &to, NetDevice::PacketType type){
                if (proto == 0x0800) // IPv4
                {
                    uint8_t buf[512];
                    pkt->CopyData (buf, pkt->GetSize ());
                    if (pkt->GetSize () > 42 && buf[23] == 17) // UDP
                    {
                        uint16_t srcPort = (buf[34] << 8) | buf[35];
                        uint16_t dstPort = (buf[36] << 8) | buf[37];
                        // DHCP DISCOVER = UDP sport 68, dport 67
                        if (srcPort == 68 || dstPort == 67)
                        {
                            pkt->EnablePrinting ();
                        }
                    }
                }
            }));
        csmaVlan[v].EnablePcap ("vlan" + std::to_string(v+1) + "_dhcp", dhcpVlanDevs[v].Get(0), true, true);
    }

    // General CSMA/point-to-point tracing
    csmaVlan[0].EnablePcapAll ("vlan1_csma");
    csmaVlan[1].EnablePcapAll ("vlan2_csma");
    csmaVlan[2].EnablePcapAll ("vlan3_csma");
    p2p.EnablePcap ("switch-router-trunk", switchRouterDevices, true);

    Simulator::Stop (Seconds (simTime));
    Simulator::Run ();
    Simulator::Destroy ();
    return 0;
}