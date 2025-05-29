#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/flow-monitor-module.h"
#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("Wifi6SpatialReuseExample");

void
LogResetEvent (std::string context)
{
  static std::ofstream logFile1 ("ap1-reset-events.log", std::ios_base::app);
  static std::ofstream logFile2 ("ap2-reset-events.log", std::ios_base::app);
  if (context.find("/NodeList/0/") != std::string::npos)
    {
      logFile1 << Simulator::Now ().GetSeconds () << "s: Spatial reuse state reset for AP1" << std::endl;
    }
  else if (context.find("/NodeList/2/") != std::string::npos)
    {
      logFile2 << Simulator::Now ().GetSeconds () << "s: Spatial reuse state reset for AP2" << std::endl;
    }
}

int
main (int argc, char *argv[])
{
  // Configurable parameters
  double d1 = 5.0;   // Distance STA1-AP1 (meters)
  double d2 = 5.0;   // STA2-AP2
  double d3 = 15.0;  // AP1-AP2
  double txPower = 20.0; // dBm
  double ccaEdThreshold = -62.0; // dBm
  double obssPdThreshold = -62.0; // dBm
  uint32_t channelWidth = 20; // MHz
  bool enableObssPd = true;
  double simTime = 10.0; // seconds
  double interval = 0.001; // seconds
  uint32_t packetSize = 1024; // bytes
  std::string phyMode = "HeMcs9"; // 802.11ax HE
  std::string dataRate = "300Mbps";

  CommandLine cmd;
  cmd.AddValue ("d1", "Distance between STA1 and AP1", d1);
  cmd.AddValue ("d2", "Distance between STA2 and AP2", d2);
  cmd.AddValue ("d3", "Distance between AP1 and AP2", d3);
  cmd.AddValue ("txPower", "Transmit power (dBm)", txPower);
  cmd.AddValue ("ccaEdThreshold", "CCA Energy Detection threshold (dBm)", ccaEdThreshold);
  cmd.AddValue ("obssPdThreshold", "OBSS PD threshold (dBm)", obssPdThreshold);
  cmd.AddValue ("channelWidth", "WiFi channel width (MHz): 20/40/80", channelWidth);
  cmd.AddValue ("enableObssPd", "Enable OBSS-PD spatial reuse", enableObssPd);
  cmd.AddValue ("simTime", "Simulation time (s)", simTime);
  cmd.AddValue ("interval", "Packet interval for traffic generator (s)", interval);
  cmd.AddValue ("packetSize", "Application packet size (bytes)", packetSize);
  cmd.Parse (argc, argv);

  LogComponentEnable ("Wifi6SpatialReuseExample", LOG_LEVEL_INFO);

  NodeContainer wifiApNodes;
  wifiApNodes.Create (2);

  NodeContainer staNodes;
  staNodes.Create (2);

  // WLAN Topology: STA1 <-> AP1   STA2 <-> AP2   AP1 <-> AP2 (d3)
  MobilityHelper mobility;

  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  positionAlloc->Add (Vector (0.0, 0.0, 0.0));           // AP1 (Node 0)
  positionAlloc->Add (Vector (d3, 0.0, 0.0));            // AP2 (Node 1)
  positionAlloc->Add (Vector (0.0, d1, 0.0));            // STA1 (Node 2)
  positionAlloc->Add (Vector (d3, d2, 0.0));             // STA2 (Node 3)
  mobility.SetPositionAllocator (positionAlloc);
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (wifiApNodes);
  mobility.Install (staNodes);

  // Wi-Fi setup
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());
  phy.Set ("TxPowerStart", DoubleValue (txPower));
  phy.Set ("TxPowerEnd", DoubleValue (txPower));
  phy.Set ("CcaEdThreshold", DoubleValue (ccaEdThreshold));

  // 802.11ax (HE) setup
  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211ax);
  WifiMacHelper mac;

  phy.Set ("ChannelWidth", UintegerValue (channelWidth));
  phy.Set ("Frequency", UintegerValue (5180)); // Primary channel = 36
  phy.Set ("RxNoiseFigure", DoubleValue (7.0));

  // Set up OBSS-PD spatial reuse
  HePhyHelpers heHelpers (phy);
  if (enableObssPd)
    {
      Config::SetDefault ("ns3::WifiPhy::ObssPdEnable", BooleanValue (true));
      Config::SetDefault ("ns3::WifiPhy::ObssPdThreshold", DoubleValue (obssPdThreshold));
    }
  else
    {
      Config::SetDefault ("ns3::WifiPhy::ObssPdEnable", BooleanValue (false));
    }

  // AP1
  Ssid ssid1 = Ssid ("bss1");
  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid1));
  NetDeviceContainer apDevice1 = wifi.Install (phy, mac, wifiApNodes.Get (0));

  // STA1
  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid1));
  NetDeviceContainer staDevice1 = wifi.Install (phy, mac, staNodes.Get (0));

  // AP2
  Ssid ssid2 = Ssid ("bss2");
  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid2));
  NetDeviceContainer apDevice2 = wifi.Install (phy, mac, wifiApNodes.Get (1));

  // STA2
  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid2));
  NetDeviceContainer staDevice2 = wifi.Install (phy, mac, staNodes.Get (1));

  // Internet stack
  InternetStackHelper stack;
  stack.Install (wifiApNodes);
  stack.Install (staNodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer ap1Ip = address.Assign (apDevice1);
  Ipv4InterfaceContainer sta1Ip = address.Assign (staDevice1);

  address.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer ap2Ip = address.Assign (apDevice2);
  Ipv4InterfaceContainer sta2Ip = address.Assign (staDevice2);

  // Disable ARP and set static routing
  Ipv4StaticRoutingHelper ipv4Routing;
  Ptr<Ipv4StaticRouting> sta1Static = ipv4Routing.GetStaticRouting (staNodes.Get (0)->GetObject<Ipv4> ());
  sta1Static->AddHostRouteTo (ap1Ip.GetAddress (0), 1);
  Ptr<Ipv4StaticRouting> sta2Static = ipv4Routing.GetStaticRouting (staNodes.Get (1)->GetObject<Ipv4> ());
  sta2Static->AddHostRouteTo (ap2Ip.GetAddress (0), 1);

  // Applications: UDP flows, continuous traffic
  uint16_t port1 = 4000;
  uint16_t port2 = 5000;
  UdpServerHelper server1 (port1);
  ApplicationContainer serverApp1 = server1.Install (wifiApNodes.Get (0));
  serverApp1.Start (Seconds (0.0));
  serverApp1.Stop (Seconds (simTime + 1));

  UdpServerHelper server2 (port2);
  ApplicationContainer serverApp2 = server2.Install (wifiApNodes.Get (1));
  serverApp2.Start (Seconds (0.0));
  serverApp2.Stop (Seconds (simTime + 1));

  UdpClientHelper client1 (ap1Ip.GetAddress (0), port1);
  client1.SetAttribute ("MaxPackets", UintegerValue (0));
  client1.SetAttribute ("Interval", TimeValue (Seconds (interval)));
  client1.SetAttribute ("PacketSize", UintegerValue (packetSize));
  ApplicationContainer clientApp1 = client1.Install (staNodes.Get (0));
  clientApp1.Start (Seconds (0.5));
  clientApp1.Stop (Seconds (simTime));

  UdpClientHelper client2 (ap2Ip.GetAddress (0), port2);
  client2.SetAttribute ("MaxPackets", UintegerValue (0));
  client2.SetAttribute ("Interval", TimeValue (Seconds (interval)));
  client2.SetAttribute ("PacketSize", UintegerValue (packetSize));
  ApplicationContainer clientApp2 = client2.Install (staNodes.Get (1));
  clientApp2.Start (Seconds (0.5));
  clientApp2.Stop (Seconds (simTime));

  // Reset event tracing (simulate spatial reuse reset logs)
  Config::Connect ("/NodeList/0/DeviceList/0/$ns3::WifiNetDevice/Phy/$ns3::WifiPhy/Reset", MakeCallback (&LogResetEvent));
  Config::Connect ("/NodeList/1/DeviceList/0/$ns3::WifiNetDevice/Phy/$ns3::WifiPhy/Reset", MakeCallback (&LogResetEvent));

  // Enable FlowMonitor for throughput statistics
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  Simulator::Stop (Seconds (simTime + 1.0));
  Simulator::Run ();

  double throughputAp1 = 0.0, throughputAp2 = 0.0;
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());

  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();
  for (auto i = stats.begin (); i != stats.end (); ++i)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
      // STA1->AP1: 10.1.1.2 -> 10.1.1.1 (UDP port 4000)
      if (t.destinationAddress == ap1Ip.GetAddress (0) && t.destinationPort == port1)
        {
          throughputAp1 += (i->second.rxBytes * 8.0) / (simTime * 1000000.0); // Mbps
        }
      // STA2->AP2: 10.1.2.2 -> 10.1.2.1 (UDP port 5000)
      if (t.destinationAddress == ap2Ip.GetAddress (0) && t.destinationPort == port2)
        {
          throughputAp2 += (i->second.rxBytes * 8.0) / (simTime * 1000000.0);
        }
    }

  std::cout << "=== Simulation Results ===" << std::endl;
  std::cout << "OBSS-PD Spatial Reuse: " << (enableObssPd ? "ENABLED" : "DISABLED") << std::endl;
  std::cout << "BSS1 (STA1->AP1) Throughput: " << throughputAp1 << " Mbps" << std::endl;
  std::cout << "BSS2 (STA2->AP2) Throughput: " << throughputAp2 << " Mbps" << std::endl;

  Simulator::Destroy ();
  return 0;
}