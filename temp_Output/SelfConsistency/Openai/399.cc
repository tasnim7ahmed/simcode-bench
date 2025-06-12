#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiUdpExample");

int
main (int argc, char *argv[])
{
  // Enable logging for UdpClient and UdpServer
  // LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
  // LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);

  // 1. Create 2 nodes
  NodeContainer wifiNodes;
  wifiNodes.Create (2);

  // 2. Create Wi-Fi channel
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());

  // 3. Set up Wi-Fi MAC and standard
  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211g);

  WifiMacHelper mac;

  Ssid ssid = Ssid ("wifi-udp-ssid");

  // Node 0 is AP (server), node 1 is STA (client)
  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid),
               "ActiveProbing", BooleanValue (false));
  NetDeviceContainer staDevice;
  staDevice = wifi.Install (phy, mac, wifiNodes.Get (1));

  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid));
  NetDeviceContainer apDevice;
  apDevice = wifi.Install (phy, mac, wifiNodes.Get (0));

  // 4. Set mobility
  MobilityHelper mobility;
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (wifiNodes);

  // 5. Install internet stack
  InternetStackHelper stack;
  stack.Install (wifiNodes);

  // 6. Assign IP addresses
  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces;
  interfaces.Add (address.Assign (apDevice));
  interfaces.Add (address.Assign (staDevice));

  // 7. Install UDP server on node 0 (AP)
  uint16_t port = 9;
  UdpServerHelper server (port);
  ApplicationContainer serverApp = server.Install (wifiNodes.Get (0));
  serverApp.Start (Seconds (1.0));
  serverApp.Stop (Seconds (10.0));

  // 8. Install UDP client on node 1 (STA)
  UdpClientHelper client (interfaces.GetAddress (0), port);
  client.SetAttribute ("MaxPackets", UintegerValue (10000)); // effectively unlimited for our short sim
  client.SetAttribute ("Interval", TimeValue (MilliSeconds (100)));
  client.SetAttribute ("PacketSize", UintegerValue (1024));

  ApplicationContainer clientApp = client.Install (wifiNodes.Get (1));
  clientApp.Start (Seconds (2.0));
  clientApp.Stop (Seconds (10.0));

  // 9. Enable pcap tracing (optional)
  phy.EnablePcap ("wifi-udp-server", apDevice.Get (0));
  phy.EnablePcap ("wifi-udp-client", staDevice.Get (0));

  // 10. Run simulation
  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}