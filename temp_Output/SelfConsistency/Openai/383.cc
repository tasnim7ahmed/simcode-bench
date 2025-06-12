#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/yans-wifi-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiUdpExample");

int main (int argc, char *argv[])
{
  Time::SetResolution (Time::NS);
  LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
  LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);

  // 1. Create nodes
  NodeContainer wifiNodes;
  wifiNodes.Create (2); // Node 0: AP, Node 1: STA

  // 2. Set up Wi-Fi, PHY, mac, and channel
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());

  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211g);

  WifiMacHelper mac;
  Ssid ssid = Ssid ("ns-3-ssid");

  NetDeviceContainer devices;

  // AP
  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid));
  devices.Add (wifi.Install (phy, mac, wifiNodes.Get (0)));

  // STA
  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid),
               "ActiveProbing", BooleanValue (false));
  devices.Add (wifi.Install (phy, mac, wifiNodes.Get (1)));

  // 3. Mobility
  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  positionAlloc->Add (Vector (0.0, 0.0, 0.0));  // AP position
  positionAlloc->Add (Vector (5.0, 0.0, 0.0));  // STA position
  mobility.SetPositionAllocator (positionAlloc);
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (wifiNodes);

  // 4. Internet stack and IP assignment
  InternetStackHelper stack;
  stack.Install (wifiNodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  // 5. UDP server on AP (Node 0)
  uint16_t port = 9;
  UdpServerHelper server (port);
  ApplicationContainer serverApps = server.Install (wifiNodes.Get (0));
  serverApps.Start (Seconds (0.0));
  serverApps.Stop (Seconds (10.0));

  // 6. UDP client on STA (Node 1) targeting AP
  uint32_t maxPacketCount = 320;
  Time interPacketInterval = Seconds (0.025); // 40 packets/sec
  UdpClientHelper client (interfaces.GetAddress (0), port);
  client.SetAttribute ("MaxPackets", UintegerValue (maxPacketCount));
  client.SetAttribute ("Interval", TimeValue (interPacketInterval));
  client.SetAttribute ("PacketSize", UintegerValue (1024));

  ApplicationContainer clientApps = client.Install (wifiNodes.Get (1));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}