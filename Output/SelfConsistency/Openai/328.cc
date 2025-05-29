#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int
main (int argc, char *argv[])
{
  // LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
  // LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

  // Create 2 nodes
  NodeContainer wifiNodes;
  wifiNodes.Create (2);

  // Configure physical and MAC layers: 802.11b
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());

  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211b);

  WifiMacHelper mac;
  Ssid ssid = Ssid ("simple-wifi");
  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid),
               "ActiveProbing", BooleanValue (false));

  NetDeviceContainer staDevices;
  staDevices = wifi.Install (phy, mac, NodeContainer (wifiNodes.Get (0)));

  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid));

  NetDeviceContainer apDevices;
  apDevices = wifi.Install (phy, mac, NodeContainer (wifiNodes.Get (1)));

  // Mobility -- constant position
  MobilityHelper mobility;
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (wifiNodes);

  // Internet protocol stack
  InternetStackHelper stack;
  stack.Install (wifiNodes);

  // Assign IP addresses
  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");

  NetDeviceContainer allDevices;
  allDevices.Add (staDevices);
  allDevices.Add (apDevices);

  Ipv4InterfaceContainer interfaces = address.Assign (allDevices);

  // UDP Server setup on Node 1 (receiver)
  uint16_t port = 50000;
  UdpServerHelper server (port);
  ApplicationContainer serverApp = server.Install (wifiNodes.Get (1));
  serverApp.Start (Seconds (1.0));
  serverApp.Stop (Seconds (10.0));

  // UDP Client setup on Node 0 (sender)
  uint32_t packetSize = 1024; // bytes
  DataRate clientDataRate ("500kbps");
  UdpClientHelper client (interfaces.GetAddress (1), port);

  client.SetAttribute ("MaxPackets", UintegerValue (4294967295u));
  client.SetAttribute ("Interval", TimeValue (Seconds (static_cast<double>(packetSize * 8) / clientDataRate.GetBitRate ())));
  client.SetAttribute ("PacketSize", UintegerValue (packetSize));

  ApplicationContainer clientApp = client.Install (wifiNodes.Get (0));
  clientApp.Start (Seconds (1.0));
  clientApp.Stop (Seconds (10.0));

  // Run simulation
  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}