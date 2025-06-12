#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiTcpRenoSimpleExample");

int
main (int argc, char *argv[])
{
  // Logging
  LogComponentEnable ("WifiTcpRenoSimpleExample", LOG_LEVEL_INFO);

  // Set TCP variant to Reno
  Config::SetDefault ("ns3::TcpL4Protocol::SocketType",
                      TypeIdValue (TcpReno::GetTypeId ()));

  // Create two nodes
  NodeContainer nodes;
  nodes.Create (2);

  // Configure Wi-Fi PHY and MAC
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());

  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211a);

  WifiMacHelper mac;
  Ssid ssid = Ssid ("simple-wifi");
  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid),
               "ActiveProbing", BooleanValue (false));

  NetDeviceContainer staDevice;
  staDevice = wifi.Install (phy, mac, nodes.Get (0)); // Station

  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid));

  NetDeviceContainer apDevice;
  apDevice = wifi.Install (phy, mac, nodes.Get (1));  // Access Point

  // Set mobility: static positions
  MobilityHelper mobility;
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (nodes);
  // Set positions
  Ptr<MobilityModel> mob1 = nodes.Get(0)->GetObject<MobilityModel> ();
  mob1->SetPosition (Vector (0.0, 0.0, 0.0));
  Ptr<MobilityModel> mob2 = nodes.Get(1)->GetObject<MobilityModel> ();
  mob2->SetPosition (Vector (5.0, 0.0, 0.0));

  // Install Internet stack
  InternetStackHelper stack;
  stack.Install (nodes);

  // Assign IP addresses
  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces;
  NetDeviceContainer allDevices;
  allDevices.Add (staDevice.Get (0));
  allDevices.Add (apDevice.Get (0));
  interfaces = address.Assign (allDevices);

  // Create and install TCP server (PacketSink) on node 1
  uint16_t port = 9;
  Address sinkAddress (InetSocketAddress (interfaces.GetAddress (1), port));
  PacketSinkHelper packetSinkHelper ("ns3::TcpSocketFactory",
                                     InetSocketAddress (Ipv4Address::GetAny (), port));
  ApplicationContainer serverApp = packetSinkHelper.Install (nodes.Get (1));
  serverApp.Start (Seconds (0.0));
  serverApp.Stop (Seconds (10.0));

  // Create and install TCP client (OnOffApplication) on node 0
  OnOffHelper clientHelper ("ns3::TcpSocketFactory", sinkAddress);
  clientHelper.SetAttribute ("PacketSize", UintegerValue (1024));
  clientHelper.SetAttribute ("DataRate", DataRateValue (DataRate ("81920bps"))); // 1024B/0.1s = 81920bps
  clientHelper.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
  clientHelper.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));

  ApplicationContainer clientApp = clientHelper.Install (nodes.Get (0));
  clientApp.Start (Seconds (1.0));
  clientApp.Stop (Seconds (10.0));

  // Run simulation
  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}