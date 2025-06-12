#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/udp-client-server-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiThreeNodeForwardingExample");

int 
main (int argc, char *argv[])
{
  Time::SetResolution (Time::NS);
  LogComponentEnable ("UdpClientHelper", LOG_LEVEL_INFO);
  LogComponentEnable ("UdpServerHelper", LOG_LEVEL_INFO);

  NodeContainer wifiStaNodes;
  wifiStaNodes.Create (1); // Node 0: STA1
  NodeContainer wifiApNode;
  wifiApNode.Create (1); // Node 1: AP
  NodeContainer wiredDstNode;
  wiredDstNode.Create (1); // Node 2: Wired destination

  // Wi-Fi PHY/MAC
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());
  
  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211g);
  wifi.SetRemoteStationManager ("ns3::AarfWifiManager");

  WifiMacHelper mac;
  Ssid ssid = Ssid ("ns3-ssid");

  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid),
               "ActiveProbing", BooleanValue (false));
  NetDeviceContainer staDevice = wifi.Install (phy, mac, wifiStaNodes);

  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid));
  NetDeviceContainer apDevice = wifi.Install (phy, mac, wifiApNode);

  // Wired CSMA (between AP and node 2)
  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", DataRateValue (DataRate ("100Mbps")));
  csma.SetChannelAttribute ("Delay", TimeValue (NanoSeconds (6560)));
  NodeContainer csmaNodes;
  csmaNodes.Add (wifiApNode.Get (0)); // AP
  csmaNodes.Add (wiredDstNode.Get (0)); // Node 2
  NetDeviceContainer csmaDevices = csma.Install (csmaNodes);

  // Mobility (all fixed positions)
  MobilityHelper mobility;
  Ptr<ListPositionAllocator> pos = CreateObject<ListPositionAllocator> ();
  pos->Add (Vector (0.0,0.0,0.0));   // STA1
  pos->Add (Vector (5.0,0.0,0.0));   // AP
  pos->Add (Vector (10.0,0.0,0.0));  // Node 2
  mobility.SetPositionAllocator (pos);
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (wifiStaNodes);
  mobility.Install (wifiApNode);
  mobility.Install (wiredDstNode);

  // Internet stack
  InternetStackHelper stack;
  stack.Install (wifiStaNodes);
  stack.Install (wifiApNode);
  stack.Install (wiredDstNode);

  // IP addressing
  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer staIf = address.Assign (staDevice);
  Ipv4InterfaceContainer apIf = address.Assign (apDevice);

  address.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer csmaIf = address.Assign (csmaDevices);

  // Static routing (STA1 -> AP -> Node2)
  Ipv4StaticRoutingHelper staticRouting;
  Ptr<Node> sta1 = wifiStaNodes.Get (0);
  Ptr<Node> ap = wifiApNode.Get (0);
  Ptr<Node> node2 = wiredDstNode.Get (0);

  Ptr<Ipv4> sta1Ipv4 = sta1->GetObject<Ipv4> ();
  Ptr<Ipv4StaticRouting> sta1StaticRouting = staticRouting.GetStaticRouting (sta1Ipv4);
  sta1StaticRouting->AddNetworkRouteTo (
    Ipv4Address("10.1.2.0"), Ipv4Mask("255.255.255.0"), apIf.GetAddress (0), 1);

  Ptr<Ipv4> apIpv4 = ap->GetObject<Ipv4> ();
  Ptr<Ipv4StaticRouting> apStaticRouting = staticRouting.GetStaticRouting (apIpv4);
  // route to wired network (already present if interface is up)
  apStaticRouting->AddNetworkRouteTo (
    Ipv4Address("10.1.2.0"), Ipv4Mask("255.255.255.0"), apIpv4->GetInterfaceForDevice(csmaDevices.Get (0)));

  // route to wifi network
  apStaticRouting->AddNetworkRouteTo (
    Ipv4Address("10.1.1.0"), Ipv4Mask("255.255.255.0"), apIpv4->GetInterfaceForDevice(apDevice.Get(0)));

  Ptr<Ipv4> node2Ipv4 = node2->GetObject<Ipv4> ();
  Ptr<Ipv4StaticRouting> node2StaticRouting = staticRouting.GetStaticRouting (node2Ipv4);
  node2StaticRouting->AddNetworkRouteTo (
    Ipv4Address("10.1.1.0"), Ipv4Mask("255.255.255.0"), csmaIf.GetAddress(0), 1);

  // UDP server (on Node2)
  uint16_t port = 4000;
  UdpServerHelper udpServer (port);
  ApplicationContainer serverApps = udpServer.Install (wiredDstNode.Get(0));
  serverApps.Start (Seconds(1.0));
  serverApps.Stop (Seconds(10.0));

  // UDP client (on Node0, STA1), sending packets to Node2 via AP
  UdpClientHelper udpClient (csmaIf.GetAddress(1), port);
  udpClient.SetAttribute ("MaxPackets", UintegerValue (320));
  udpClient.SetAttribute ("Interval", TimeValue (Seconds(0.03)));
  udpClient.SetAttribute ("PacketSize", UintegerValue (1024));
  ApplicationContainer clientApps = udpClient.Install (wifiStaNodes.Get(0));
  clientApps.Start (Seconds(2.0));
  clientApps.Stop (Seconds(10.0));

  Simulator::Stop (Seconds (10.0));

  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}