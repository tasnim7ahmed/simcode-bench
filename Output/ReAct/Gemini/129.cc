#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/dhcp-server-helper.h"
#include "ns3/dhcp-client-helper.h"
#include "ns3/vlan-tag.h"
#include "ns3/tap-bridge-module.h"
#include "ns3/pcap-file.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("VlanRoutingDhcp");

int main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);

  NodeContainer hostsVlan1, hostsVlan2, hostsVlan3;
  hostsVlan1.Create (1);
  hostsVlan2.Create (1);
  hostsVlan3.Create (1);

  NodeContainer switchNode;
  switchNode.Create (1);

  NodeContainer routerNode;
  routerNode.Create (1);

  InternetStackHelper stack;
  stack.Install (hostsVlan1);
  stack.Install (hostsVlan2);
  stack.Install (hostsVlan3);
  stack.Install (routerNode);
  stack.Install (switchNode);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer switchRouterDevices;
  switchRouterDevices = p2p.Install (switchNode.Get (0), routerNode.Get (0));

  NetDeviceContainer switchVlan1Devices, switchVlan2Devices, switchVlan3Devices;
  switchVlan1Devices = p2p.Install (switchNode.Get (0), hostsVlan1.Get (0));
  switchVlan2Devices = p2p.Install (switchNode.Get (0), hostsVlan2.Get (0));
  switchVlan3Devices = p2p.Install (switchNode.Get (0), hostsVlan3.Get (0));

  // VLAN configuration on switch
  VlanTag tag1 = VlanTag (10);
  VlanTag tag2 = VlanTag (20);
  VlanTag tag3 = VlanTag (30);

  switchVlan1Devices.Get (1)->SetAttribute ("VlanTag", VlanTagValue (tag1));
  switchVlan2Devices.Get (1)->SetAttribute ("VlanTag", VlanTagValue (tag2));
  switchVlan3Devices.Get (1)->SetAttribute ("VlanTag", VlanTagValue (tag3));

  // Install internet stack on switch and router (needed for forwarding)

  // Assign IP addresses
  Ipv4AddressHelper address;

  // VLAN 1 subnet
  address.SetBase ("10.1.10.0", "255.255.255.0");
  Ipv4InterfaceContainer vlan1Interfaces;
  vlan1Interfaces = address.Assign (switchVlan1Devices);

  // VLAN 2 subnet
  address.SetBase ("10.1.20.0", "255.255.255.0");
  Ipv4InterfaceContainer vlan2Interfaces;
  vlan2Interfaces = address.Assign (switchVlan2Devices);

  // VLAN 3 subnet
  address.SetBase ("10.1.30.0", "255.255.255.0");
  Ipv4InterfaceContainer vlan3Interfaces;
  vlan3Interfaces = address.Assign (switchVlan3Devices);

  // Router subnet
  address.SetBase ("192.168.1.0", "255.255.255.0");
  Ipv4InterfaceContainer routerSwitchInterfaces;
  routerSwitchInterfaces = address.Assign (switchRouterDevices);

  // DHCP Server configuration

  DhcpServerHelper dhcpServerHelper;
  dhcpServerHelper.Enable ();

  // Configure DHCP scopes for each VLAN
  dhcpServerHelper.AddAddressPool (Ipv4Address ("10.1.10.10"), Ipv4Address ("10.1.10.20"), Ipv4Mask ("255.255.255.0"), routerSwitchInterfaces.GetAddress(1));
  dhcpServerHelper.AddAddressPool (Ipv4Address ("10.1.20.10"), Ipv4Address ("10.1.20.20"), Ipv4Mask ("255.255.255.0"), routerSwitchInterfaces.GetAddress(1));
  dhcpServerHelper.AddAddressPool (Ipv4Address ("10.1.30.10"), Ipv4Address ("10.1.30.20"), Ipv4Mask ("255.255.255.0"), routerSwitchInterfaces.GetAddress(1));

  ApplicationContainer dhcpServerApp = dhcpServerHelper.Install (routerNode.Get (0));
  dhcpServerApp.Start (Seconds (1.0));
  dhcpServerApp.Stop (Seconds (10.0));

  // DHCP Client configuration
  DhcpClientHelper dhcpClientHelper;
  ApplicationContainer dhcpClientAppsVlan1 = dhcpClientHelper.Install (hostsVlan1.Get (0));
  dhcpClientAppsVlan1.Start (Seconds (2.0));
  dhcpClientAppsVlan1.Stop (Seconds (9.0));

  ApplicationContainer dhcpClientAppsVlan2 = dhcpClientHelper.Install (hostsVlan2.Get (0));
  dhcpClientAppsVlan2.Start (Seconds (2.0));
  dhcpClientAppsVlan2.Stop (Seconds (9.0));

  ApplicationContainer dhcpClientAppsVlan3 = dhcpClientHelper.Install (hostsVlan3.Get (0));
  dhcpClientAppsVlan3.Start (Seconds (2.0));
  dhcpClientAppsVlan3.Stop (Seconds (9.0));

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // Add static routes on the router for inter-VLAN routing
  Ptr<Ipv4StaticRouting> staticRouting = Ipv4RoutingHelper::GetStaticRouting (routerNode.Get (0)->GetObject<Ipv4> ());
  staticRouting->AddNetworkRouteTo (Ipv4Address ("10.1.10.0"), Ipv4Mask ("255.255.255.0"), 1);
  staticRouting->AddNetworkRouteTo (Ipv4Address ("10.1.20.0"), Ipv4Mask ("255.255.255.0"), 1);
  staticRouting->AddNetworkRouteTo (Ipv4Address ("10.1.30.0"), Ipv4Mask ("255.255.255.0"), 1);

  // Traffic generation (UDP traffic from VLAN1 host to VLAN2 host)
  UdpClientHelper client (vlan2Interfaces.GetAddress (0), 9);
  client.SetAttribute ("MaxPackets", UintegerValue (10));
  client.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  client.SetAttribute ("PacketSize", UintegerValue (1024));
  ApplicationContainer clientApps = client.Install (hostsVlan1.Get (0));
  clientApps.Start (Seconds (3.0));
  clientApps.Stop (Seconds (8.0));

  UdpServerHelper echoServer (9);
  ApplicationContainer serverApps = echoServer.Install (hostsVlan2.Get (0));
  serverApps.Start (Seconds (2.0));
  serverApps.Stop (Seconds (9.0));

  // Pcap Tracing

  p2p.EnablePcapAll ("vlan_routing_dhcp");

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}