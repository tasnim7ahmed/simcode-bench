#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiInfrastructureSimulation");

int main (int argc, char *argv[])
{
  CommandLine cmd (__FILE__);
  cmd.Parse (argc, argv);

  // Create nodes: 3 STAs + 1 AP
  NodeContainer staNodes;
  staNodes.Create (3);
  NodeContainer apNode;
  apNode.Create (1);

  // Setup Wi-Fi
  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211n_5GHZ);

  // Set up PHY and channel using default settings
  YansWifiPhyHelper phy;
  phy.SetPreambleDetectionModel ("ns3::ThresholdPreambleDetectionModel");
  phy.Set ("CcaEdThreshold", DoubleValue (-65));
  phy.Set ("RxGain", DoubleValue (0));
  phy.Set ("TxGain", DoubleValue (0));

  WifiChannelHelper channel = WifiChannelHelper::Default ();
  phy.SetChannel (channel.Create ());

  // MAC layer configuration
  WifiMacHelper mac;
  Ssid ssid = Ssid ("wifi-infra");

  // Setup AP
  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid),
               "BeaconGeneration", BooleanValue (true),
               "BeaconInterval", TimeValue (Seconds (2.5)));
  NetDeviceContainer apDevice;
  apDevice = wifi.Install (phy, mac, apNode);

  // Setup STAs
  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid),
               "ActiveProbing", BooleanValue (false));
  NetDeviceContainer staDevices;
  staDevices = wifi.Install (phy, mac, staNodes);

  // Mobility models
  MobilityHelper mobility;

  // AP is stationary
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (0.0),
                                 "DeltaY", DoubleValue (0.0),
                                 "GridWidth", UintegerValue (1),
                                 "LayoutType", StringValue ("RowFirst"));
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (apNode);

  // Random mobility for STAs
  mobility.SetPositionAllocator ("ns3::RandomRectanglePositionAllocator",
                                 "X", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                 "Y", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"));
  mobility.SetMobilityModel ("ns3::RandomWalk2DMobilityModel",
                             "Bounds", RectangleValue (Rectangle (0, 100, 0, 100)));
  mobility.Install (staNodes);

  // Install Internet stack
  InternetStackHelper stack;
  stack.InstallAll ();

  // Assign IP addresses
  Ipv4AddressHelper address;
  address.SetBase ("192.168.1.0", "255.255.255.0");
  Ipv4InterfaceContainer apInterface;
  apInterface = address.Assign (apDevice);
  Ipv4InterfaceContainer staInterfaces;
  staInterfaces = address.Assign (staDevices);

  // UDP Echo Server on AP
  UdpEchoServerHelper echoServer (9);
  ApplicationContainer serverApps = echoServer.Install (apNode.Get (0));
  serverApps.Start (Seconds (0.0));
  serverApps.Stop (Seconds (10.0));

  // UDP Echo Client on one STA (e.g., first STA)
  UdpEchoClientHelper echoClient (apInterface.GetAddress (0), 9);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (5));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (512));
  ApplicationContainer clientApps = echoClient.Install (staNodes.Get (0));
  clientApps.Start (Seconds (2.0));  // Start after association
  clientApps.Stop (Seconds (10.0));

  // Enable global routing
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // Simulation duration
  Simulator::Stop (Seconds (10.0));

  // Run simulation
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}