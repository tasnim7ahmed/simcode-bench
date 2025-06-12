#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiUdpStaToApExample");

int
main (int argc, char *argv[])
{
  // Enable logging for UdpClient and UdpServer (uncomment if needed)
  // LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
  // LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);

  // Create two nodes: one STA (sender), one AP (receiver)
  NodeContainer wifiNodes;
  wifiNodes.Create (2);

  Ptr<Node> sender = wifiNodes.Get (0);   // STA
  Ptr<Node> receiver = wifiNodes.Get (1); // AP

  // Configure Wi-Fi 802.11n 5GHz
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());

  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211n_5GHZ);

  WifiMacHelper mac;

  Ssid ssid = Ssid ("ns3-80211n-5ghz");

  // AP
  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid));
  NetDeviceContainer apDevice;
  apDevice = wifi.Install (phy, mac, receiver);

  // STA
  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid),
               "ActiveProbing", BooleanValue (false));
  NetDeviceContainer staDevice;
  staDevice = wifi.Install (phy, mac, sender);

  // Mobility Model
  MobilityHelper mobility;

  // STA mobility: random walk in 100x100m
  mobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                             "Bounds", RectangleValue (Rectangle (0.0, 100.0, 0.0, 100.0)),
                             "Distance", DoubleValue (10.0),
                             "Speed", StringValue ("ns3::ConstantRandomVariable[Constant=1.0]"));
  mobility.Install (sender);

  // AP mobility: stationary
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (receiver);

  // Internet stack
  InternetStackHelper stack;
  stack.Install (wifiNodes);

  // Assign IP addresses
  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces;
  interfaces = address.Assign (NetDeviceContainer (staDevice, apDevice));
  Ipv4Address apAddress = interfaces.GetAddress (1);

  // UDP server on AP
  uint16_t port = 9;
  UdpServerHelper server (port);
  ApplicationContainer serverApp = server.Install (receiver);
  serverApp.Start (Seconds (0.0));
  serverApp.Stop (Seconds (10.0));

  // UDP client on STA
  uint32_t packetSize = 1024;
  uint32_t numPackets = 10;
  double interval = 1.0; // seconds

  UdpClientHelper client (apAddress, port);
  client.SetAttribute ("MaxPackets", UintegerValue (numPackets));
  client.SetAttribute ("Interval", TimeValue (Seconds (interval)));
  client.SetAttribute ("PacketSize", UintegerValue (packetSize));

  ApplicationContainer clientApp = client.Install (sender);
  clientApp.Start (Seconds (1.0));
  clientApp.Stop (Seconds (10.0));

  // Enable pcap tracing (optional, uncomment if desired)
  // phy.EnablePcapAll ("wifi-udp-sta-to-ap");

  Simulator::Stop (Seconds (11.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}