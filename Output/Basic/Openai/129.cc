#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/bridge-module.h"
#include "ns3/csma-module.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/netanim-module.h"
#include "ns3/applications-module.h"
#include "ns3/dhcp-helper.h"
#include "ns3/vlan-tag.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("VlanRouterDhcpExample");

int
main (int argc, char *argv[])
{
  LogComponentEnable ("Ipv4L3Protocol", LOG_LEVEL_WARN);
  LogComponentEnable ("DhcpServer", LOG_LEVEL_INFO);
  LogComponentEnable ("DhcpClient", LOG_LEVEL_INFO);

  // Parameters
  uint32_t nVlan = 3;
  uint32_t hostsPerVlan = 2;

  // VLAN and subnet setup
  uint32_t vlanIds[] = {10, 20, 30};
  std::string subnets[] = {"10.1.10.0", "10.1.20.0", "10.1.30.0"};
  std::string netmasks[] = {"255.255.255.0", "255.255.255.0", "255.255.255.0"};
  std::string gw[] = {"10.1.10.1", "10.1.20.1", "10.1.30.1"};

  // Create all nodes
  NodeContainer routerNode;
  routerNode.Create (1);

  NodeContainer switchNode;
  switchNode.Create (1);

  NodeContainer hostNodes[nVlan];
  for (uint32_t i = 0; i < nVlan; ++i)
    {
      hostNodes[i].Create (hostsPerVlan);
    }

  // Create Csma links for each VLAN: hosts <-> switch
  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", StringValue ("100Mbps"));
  csma.SetChannelAttribute ("Delay", TimeValue (NanoSeconds (6560)));

  NetDeviceContainer hostDevs[nVlan];
  NetDeviceContainer switchVlanPorts[nVlan];

  for (uint32_t i = 0; i < nVlan; ++i)
    {
      NetDeviceContainer link = csma.Install (NodeContainer (hostNodes[i], switchNode));
      for (uint32_t j = 0; j < hostsPerVlan; ++j)
        {
          hostDevs[i].Add (link.Get (j));
        }
      // The switch port for this VLAN is the last device
      switchVlanPorts[i].Add (link.Get (link.GetN () - 1));
    }

  // Switch <-> Router (trunk link)
  NetDeviceContainer routerPort, switchTrunkPort;
  NetDeviceContainer trunkLink = csma.Install (NodeContainer (routerNode, switchNode));
  routerPort.Add (trunkLink.Get (0));
  switchTrunkPort.Add (trunkLink.Get (1));

  // Configure VLANs on switch (use bridge as L3 switch, with VLAN filtering)
  BridgeHelper bridge;
  NetDeviceContainer bridgeDevices;
  for (uint32_t i = 0; i < nVlan; ++i)
    {
      switchVlanPorts[i].Get (0)->SetAttribute ("VlanId", UintegerValue (vlanIds[i]));
      bridgeDevices.Add (switchVlanPorts[i]);
    }
  bridgeDevices.Add (switchTrunkPort);
  Ptr<Node> sw = switchNode.Get (0);
  bridge.Install (sw, bridgeDevices);

  // Configure 802.1Q VLAN tagging on hosts (tag outgoing frames)
  for (uint32_t i = 0; i < nVlan; ++i)
    for (uint32_t j = 0; j < hostsPerVlan; ++j)
      {
        Ptr<NetDevice> dev = hostDevs[i].Get (j);
        dev->SetAttribute ("VlanId", UintegerValue (vlanIds[i]));
      }

  // Configure subinterfaces on router (one for each VLAN on trunk port)
  NetDeviceContainer routerVlanDevs[nVlan];
  for (uint32_t i = 0; i < nVlan; ++i)
    {
      Ptr<Node> r = routerNode.Get (0);
      Ptr<NetDevice> trunk = routerPort.Get (0);
      Ptr<VlanNetDevice> vlanSubif = CreateObject<VlanNetDevice> ();
      vlanSubif->SetIfIndex (i+1);
      vlanSubif->SetAddress (Mac48Address::Allocate ());
      vlanSubif->SetVlanId (vlanIds[i]);
      vlanSubif->SetMtu (1500);
      vlanSubif->SetParent (trunk);
      r->AddDevice (vlanSubif);
      routerVlanDevs[i].Add (vlanSubif);
    }

  // Internet stack
  InternetStackHelper internet;
  for (uint32_t i = 0; i < nVlan; ++i)
    {
      internet.Install (hostNodes[i]);
    }
  internet.Install (routerNode);

  // IP addressing for router VLAN interfaces
  for (uint32_t i = 0; i < nVlan; ++i)
    {
      Ipv4AddressHelper ipv4;
      ipv4.SetBase (subnets[i].c_str (), netmasks[i].c_str ());
      // .1 is router
      ipv4.Assign (routerVlanDevs[i]);
    }

  // ---------- DHCP Server Setup ----------
  Ptr<Node> dhcpServerNode = routerNode.Get (0); // the router acts as DHCP server for all VLANs

  std::vector<Ipv4Address> poolStart, poolEnd;
  for (uint32_t i = 0; i < nVlan; ++i)
    {
      Ipv4Address base (subnets[i].c_str ());
      uint8_t data[4];
      base.Serialize (data);
      data[3] += 50; // start at .50
      poolStart.push_back (Ipv4Address (data));
      data[3] = 200; // end at .200
      poolEnd.push_back (Ipv4Address (data));
    }

  // Assign DHCP server to listen on each subinterface
  DhcpServerHelper dhcp;
  for (uint32_t i = 0; i < nVlan; ++i)
    {
      dhcp.SetSubnet (Ipv4Address (subnets[i].c_str ()), Ipv4Mask (netmasks[i].c_str ()));
      dhcp.SetPool (poolStart[i], poolEnd[i]);
      dhcp.SetRouter (Ipv4Address (gw[i].c_str ()));
      dhcp.SetInterface (routerVlanDevs[i].Get (0));
      dhcp.Install (dhcpServerNode);
    }

  // ---------- DHCP Clients on hosts ----------
  for (uint32_t i = 0; i < nVlan; ++i)
    for (uint32_t j = 0; j < hostsPerVlan; ++j)
      {
        DhcpClientHelper dhcpClient;
        dhcpClient.SetInterface (hostDevs[i].Get (j));
        dhcpClient.Install (hostNodes[i].Get (j));
      }

  // Enable pcap tracing for DHCP messages (on router)
  routerPort.Get (0)->TraceConnectWithoutContext (
      "PromiscSniffer",
      MakeCallback ([] (Ptr<const Packet> pkt) {
        PcapFileWrapper file ("dhcp-trace-router.pcap", std::ios::out | std::ios::binary);
        file.Write (Simulator::Now (), pkt);
      }));

  // Assign IP addresses (static) for DHCP server (router)
  for (uint32_t i = 0; i < nVlan; ++i)
    {
      Ptr<NetDevice> vlanDev = routerVlanDevs[i].Get (0);
      Ptr<Ipv4> ipv4 = routerNode.Get (0)->GetObject<Ipv4> ();
      int32_t ifIndex = routerNode.Get (0)->GetDeviceIndex (vlanDev);
      Ipv4InterfaceAddress ifAddr (Ipv4Address (gw[i].c_str ()), Ipv4Mask (netmasks[i].c_str ()));
      ipv4->AddAddress (ifIndex, ifAddr);
      ipv4->SetUp (ifIndex);
    }

  // Setup static routes on router (as DHCP assigns default route to hosts)
  Ipv4StaticRoutingHelper ipv4RouteHelper;
  Ptr<Ipv4> ipv4 = routerNode.Get (0)->GetObject<Ipv4> ();
  Ptr<Ipv4StaticRouting> staticRouting = ipv4RouteHelper.GetStaticRouting (ipv4);
  for (uint32_t i = 0; i < nVlan; ++i)
    {
      staticRouting->AddNetworkRouteTo (
          Ipv4Address (subnets[i].c_str ()),
          Ipv4Mask (netmasks[i].c_str ()), 
          ipv4->GetInterfaceForDevice (routerVlanDevs[i].Get (0)));
    }
  // Default route on hosts provided by DHCP

  // ---------- Applications: test inter-VLAN traffic ----------
  // vlan0 host0 acts as client, vlan1 host0 as server
  uint16_t port = 9000;
  Address anyAddress (InetSocketAddress (Ipv4Address::GetAny (), port));
  // Install server on VLAN 2 host 0
  UdpServerHelper server (port);
  ApplicationContainer serverApps = server.Install (hostNodes[1].Get (0));
  serverApps.Start (Seconds (6.0));
  serverApps.Stop (Seconds (20.0));

  // Install client on VLAN 1 host 0 (after DHCP address assigned)
  UdpClientHelper client (Ipv4Address ("10.1.20.50"), port);
  client.SetAttribute ("MaxPackets", UintegerValue (5));
  client.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  client.SetAttribute ("PacketSize", UintegerValue (64));
  ApplicationContainer clientApps = client.Install (hostNodes[0].Get (0));
  clientApps.Start (Seconds (10.0));
  clientApps.Stop (Seconds (20.0));

  // Enable PCAP tracing for DHCP/DHCP traffic
  for (uint32_t i = 0; i < nVlan; ++i)
    for (uint32_t j = 0; j < hostsPerVlan; ++j)
      {
        csma.EnablePcap ("vlan" + std::to_string (vlanIds[i]) + "-host" + std::to_string (j), hostDevs[i].Get (j), true, true);
      }
  csma.EnablePcap ("router-trunk", routerPort.Get (0), true, true);

  // Run simulation
  Simulator::Stop (Seconds (25.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}