#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/aodv-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int
main (int argc, char *argv[])
{
  // LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
  // LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

  // Set simulation parameters
  uint32_t numNodes = 10;
  double simTime = 20.0;
  double nodeSpeed = 5.0;    // meters/second
  double pauseTime = 2.0;    // seconds
  double areaSize = 500.0;   // 500m x 500m

  // Seed manager (optional)
  SeedManager::SetSeed (1);

  // Create nodes
  NodeContainer nodes;
  nodes.Create (numNodes);

  // Set up Wifi (802.11b AdHoc)
  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211b);

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
  wifiPhy.SetChannel (wifiChannel.Create ());

  WifiMacHelper wifiMac;
  wifiMac.SetType ("ns3::AdhocWifiMac");

  NetDeviceContainer devices = wifi.Install (wifiPhy, wifiMac, nodes);

  // Mobility
  MobilityHelper mobility;
  mobility.SetMobilityModel ("ns3::RandomWaypointMobilityModel",
                             "Speed", StringValue ("ns3::ConstantRandomVariable[Constant=5.0]"),
                             "Pause", StringValue ("ns3::ConstantRandomVariable[Constant=2.0]"),
                             "PositionAllocator", 
                             StringValue ("ns3::RandomRectanglePositionAllocator[MinX=0.0|MinY=0.0|MaxX=500.0|MaxY=500.0]"));
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (50.0),
                                 "DeltaY", DoubleValue (50.0),
                                 "GridWidth", UintegerValue (5),
                                 "LayoutType", StringValue ("RowFirst"));
  mobility.Install (nodes);

  // Internet stack with AODV
  AodvHelper aodv;
  InternetStackHelper internet;
  internet.SetRoutingHelper (aodv);
  internet.Install (nodes);

  // Assign IP addresses
  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.0.0", "255.255.0.0");
  Ipv4InterfaceContainer interfaces = ipv4.Assign (devices);

  // UDP server on node 0
  uint16_t serverPort = 4000;
  UdpServerHelper server (serverPort);
  ApplicationContainer serverApps = server.Install (nodes.Get (0));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (simTime));

  // UDP client on node N-1 (node 9), targets server at node 0
  UdpClientHelper client (interfaces.GetAddress (0), serverPort);
  client.SetAttribute ("MaxPackets", UintegerValue (20)); // In 20s, 1/s
  client.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  client.SetAttribute ("PacketSize", UintegerValue (1024));

  ApplicationContainer clientApps = client.Install (nodes.Get (numNodes - 1));
  clientApps.Start (Seconds (2.0)); // Start after server
  clientApps.Stop (Seconds (simTime));

  // Enable PCAP tracing
  wifiPhy.SetPcapDataLinkType (WifiPhyHelper::DLT_IEEE802_11_RADIO);
  wifiPhy.EnablePcap ("manet-aodv", devices);

  // Run simulation
  Simulator::Stop (Seconds (simTime));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}