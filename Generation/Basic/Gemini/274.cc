#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/aodv-helper.h"

// Set up logging
NS_LOG_COMPONENT_DEFINE ("ManetAodvRandomWalk");

int
main (int argc, char *argv[])
{
  // Configure logging
  LogComponentEnable ("AodvRoutingProtocol", LOG_LEVEL_ALL);
  LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
  LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);

  // Create 5 nodes
  NodeContainer nodes;
  nodes.Create (5);

  // Set up mobility: Random Walk 2D within 100x100 area
  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::RandomRectanglePositionAllocator",
                                 "X", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                 "Y", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"));
  mobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                             "Bounds", RectangleValue (Rectangle (0, 100, 0, 100)),
                             "Speed", StringValue ("ns3::ConstantRandomVariable[Constant=1.0]"), // 1 m/s constant speed
                             "Pause", StringValue ("ns3::ConstantRandomVariable[Constant=0.0]")); // No pause
  mobility.Install (nodes);

  // Set up WiFi (802.11b ad-hoc)
  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211b);

  YansWifiPhyHelper wifiPhy;
  wifiPhy.SetPcapDataLinkType (WifiPhyHelper::DLT_IEEE802_11_RADIO);
  wifiPhy.Set ("TxPowerStart", DoubleValue (7.5)); // Tx power for 802.11b
  wifiPhy.Set ("TxPowerEnd", DoubleValue (7.5));

  YansWifiChannelHelper wifiChannel;
  wifiChannel.SetPropagationLossModel ("ns3::FriisPropagationLossModel");
  wifiChannel.SetPropagationDelayModel ("ns3::ConstantSpeedPropagationDelayModel");
  wifiPhy.SetChannel (wifiChannel.Create ());

  WifiMacHelper wifiMac;
  wifiMac.SetType ("ns3::AdhocWifiMac");

  NetDeviceContainer devices = wifi.Install (wifiPhy, wifiMac, nodes);

  // Install AODV routing protocol
  AodvHelper aodv;
  InternetStackHelper internet;
  internet.SetRoutingHelper (aodv);
  internet.Install (nodes);

  // Assign IP addresses (10.1.1.0/24 subnet)
  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = ipv4.Assign (devices);

  // Create UDP Echo Server on Node 4
  UdpEchoServerHelper echoServer (9); // Port 9
  ApplicationContainer serverApps = echoServer.Install (nodes.Get (4));
  serverApps.Start (Seconds (1.0)); // Start server at 1.0 second
  serverApps.Stop (Seconds (10.0));

  // Create UDP Echo Client on Node 0
  // Get IP address of Node 4
  Ptr<Ipv4> ipv4Node4 = nodes.Get (4)->GetObject<Ipv4> ();
  Ipv4Address node4Ip = ipv4Node4->GetAddress (1, 0).GetLocal (); // Get first IP address on the first interface

  UdpEchoClientHelper echoClient (node4Ip, 9);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (5)); // Five 512-byte packets
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0))); // 1-second intervals
  echoClient.SetAttribute ("PacketSize", UintegerValue (512)); // 512 bytes

  ApplicationContainer clientApps = echoClient.Install (nodes.Get (0));
  clientApps.Start (Seconds (1.0)); // Start client at 1.0 second
  clientApps.Stop (Seconds (10.0));

  // Set simulation duration
  Simulator::Stop (Seconds (10.0));

  // Run simulation
  NS_LOG_INFO ("Starting simulation.");
  Simulator::Run ();
  NS_LOG_INFO ("Simulation finished.");

  // Clean up
  Simulator::Destroy ();

  return 0;
}