#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/dhcp-helper.h"
#include "ns3/vlan-module.h"
#include "ns3/net-device.h"
#include "ns3/packet.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("VlanRoutingDhcp");

int main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);
  LogComponent::SetLogLevel (LOG_LEVEL_INFO);

  // Create nodes
  NodeContainer vlanswitch;
  vlanswitch.Create (1);
  NodeContainer router;
  router.Create (1);
  NodeContainer host1, host2, host3;
  host1.Create (1);
  host2.Create (1);
  host3.Create (1);

  // Create point-to-point links
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer switchRouterDevices = p2p.Install (vlanswitch.Get (0), router.Get (0));
  NetDeviceContainer switchHost1Devices = p2p.Install (vlanswitch.Get (0), host1.Get (0));
  NetDeviceContainer switchHost2Devices = p2p.Install (vlanswitch.Get (0), host2.Get (0));
  NetDeviceContainer switchHost3Devices = p2p.Install (vlanswitch.Get (0), host3.Get (0));

  // Install Internet stack
  InternetStackHelper stack;
  stack.Install (router);
  stack.Install (host1);
  stack.Install (host2);
  stack.Install (host3);

  // Configure VLANs
  VlanTag tag1 = VlanTag (10, 0);
  VlanTag tag2 = VlanTag (20, 0);
  VlanTag tag3 = VlanTag (30, 0);

  VlanHelper vlanHelper;

  NetDeviceContainer vlanDevices1 = vlanHelper.AssignVlanId (switchHost1Devices.Get (1), tag1.GetVlanId ());
  NetDeviceContainer vlanDevices2 = vlanHelper.AssignVlanId (switchHost2Devices.Get (1), tag2.GetVlanId ());
  NetDeviceContainer vlanDevices3 = vlanHelper.AssignVlanId (switchHost3Devices.Get (1), tag3.GetVlanId ());

  NetDeviceContainer vlanSwitchDevice1;
  vlanSwitchDevice1.Add (switchHost1Devices.Get (0));
  NetDeviceContainer vlanSwitchDevice2;
  vlanSwitchDevice2.Add (switchHost2Devices.Get (0));
  NetDeviceContainer vlanSwitchDevice3;
  vlanSwitchDevice3.Add (switchHost3Devices.Get (0));

  // Assign IP addresses
  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer routerInterfaces1 = address.Assign (switchRouterDevices.Get (1));

  address.SetBase ("10.1.10.0", "255.255.255.0");
  Ipv4InterfaceContainer hostInterfaces1 = address.Assign (vlanDevices1);

  address.SetBase ("10.1.20.0", "255.255.255.0");
  Ipv4InterfaceContainer hostInterfaces2 = address.Assign (vlanDevices2);

  address.SetBase ("10.1.30.0", "255.255.255.0");
  Ipv4InterfaceContainer hostInterfaces3 = address.Assign (vlanDevices3);

  //DHCP Server for VLAN 10
  DhcpServerHelper dhcpServer1;
  dhcpServer1.EnableCallbacks ();
  Ipv4Address dhcpStart1 ("10.1.10.2");
  dhcpServer1.SetAddressPool (Ipv4Address ("10.1.10.0"), Ipv4Mask ("255.255.255.0"), dhcpStart1, Ipv4Address ("10.1.10.254"));
  dhcpServer1.Install (vlanswitch.Get (0), switchHost1Devices.Get(0));

  //DHCP Server for VLAN 20
  DhcpServerHelper dhcpServer2;
  dhcpServer2.EnableCallbacks ();
  Ipv4Address dhcpStart2 ("10.1.20.2");
  dhcpServer2.SetAddressPool (Ipv4Address ("10.1.20.0"), Ipv4Mask ("255.255.255.0"), dhcpStart2, Ipv4Address ("10.1.20.254"));
  dhcpServer2.Install (vlanswitch.Get (0), switchHost2Devices.Get(0));

    //DHCP Server for VLAN 30
  DhcpServerHelper dhcpServer3;
  dhcpServer3.EnableCallbacks ();
  Ipv4Address dhcpStart3 ("10.1.30.2");
  dhcpServer3.SetAddressPool (Ipv4Address ("10.1.30.0"), Ipv4Mask ("255.255.255.0"), dhcpStart3, Ipv4Address ("10.1.30.254"));
  dhcpServer3.Install (vlanswitch.Get (0), switchHost3Devices.Get(0));


  DhcpClientHelper dhcpClient;
  dhcpClient.Install (host1.Get (0), vlanDevices1.Get(0));
  dhcpClient.Install (host2.Get (0), vlanDevices2.Get(0));
  dhcpClient.Install (host3.Get (0), vlanDevices3.Get(0));

  // Enable forwarding on the router
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // Create traffic
  UdpEchoClientHelper echoClient (host3.Get (0)->GetAddress (1,0)->GetLocal (), 9);
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));
  echoClient.Install (host1.Get (0)).Start (Seconds (2.0));

  // Add static route on each host to the router
  Ptr<Ipv4StaticRouting> staticRouting1 = Ipv4RoutingHelper::GetRouting <Ipv4StaticRouting> (host1.Get (0)->GetObject<Ipv4> ()->GetRoutingProtocol ());
  staticRouting1->SetDefaultRoute (routerInterfaces1.GetAddress (1), 1);

  Ptr<Ipv4StaticRouting> staticRouting2 = Ipv4RoutingHelper::GetRouting <Ipv4StaticRouting> (host2.Get (0)->GetObject<Ipv4> ()->GetRoutingProtocol ());
  staticRouting2->SetDefaultRoute (routerInterfaces1.GetAddress (1), 1);

  Ptr<Ipv4StaticRouting> staticRouting3 = Ipv4RoutingHelper::GetRouting <Ipv4StaticRouting> (host3.Get (0)->GetObject<Ipv4> ()->GetRoutingProtocol ());
  staticRouting3->SetDefaultRoute (routerInterfaces1.GetAddress (1), 1);


  // Enable PCAP Tracing for DHCP traffic
  p2p.EnablePcapAll ("vlan_dhcp");

  // Run simulation
  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}