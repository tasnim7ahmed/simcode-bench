#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/yans-wifi-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("SimpleWifiUdpExample");

int
main (int argc, char *argv[])
{
  // Enable logging for UdpClient and UdpServer (optional)
  // LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
  // LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);

  // Set simulation parameters
  uint32_t numPackets = 1000;
  Time interPacketInterval = MilliSeconds (10);
  uint16_t serverPort = 9; // Well-known echo port

  // Create nodes
  NodeContainer wifiNodes;
  wifiNodes.Create (2);

  // Set up WiFi PHY and channel
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());

  // Set WiFi standard to 802.11b
  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211b);

  WifiMacHelper mac;
  Ssid ssid = Ssid ("wifi-network");

  // Set MAC to STA for one node, AP for the other
  NetDeviceContainer devices;

  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid),
               "ActiveProbing", BooleanValue (false));
  devices.Add (wifi.Install (phy, mac, wifiNodes.Get (0)));

  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid));
  devices.Add (wifi.Install (phy, mac, wifiNodes.Get (1)));

  // Set up mobility
  MobilityHelper mobility;
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (wifiNodes);

  // Position nodes apart
  Ptr<MobilityModel> mob0 = wifiNodes.Get (0)->GetObject<MobilityModel> ();
  mob0->SetPosition (Vector (0, 0, 0));
  Ptr<MobilityModel> mob1 = wifiNodes.Get (1)->GetObject<MobilityModel> ();
  mob1->SetPosition (Vector (5, 0, 0));

  // Install Internet stack
  InternetStackHelper stack;
  stack.Install (wifiNodes);

  // Assign IP addresses
  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  // Set up UDP server on AP node (node 1)
  UdpServerHelper server (serverPort);
  ApplicationContainer serverApp = server.Install (wifiNodes.Get (1));
  serverApp.Start (Seconds (1.0));
  serverApp.Stop (Seconds (15.0));

  // Set up UDP client on STA node (node 0)
  UdpClientHelper client (interfaces.GetAddress (1), serverPort);
  client.SetAttribute ("MaxPackets", UintegerValue (numPackets));
  client.SetAttribute ("Interval", TimeValue (interPacketInterval));
  client.SetAttribute ("PacketSize", UintegerValue (1024));
  ApplicationContainer clientApp = client.Install (wifiNodes.Get (0));
  clientApp.Start (Seconds (2.0));
  clientApp.Stop (Seconds (15.0));

  Simulator::Stop (Seconds (16.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}