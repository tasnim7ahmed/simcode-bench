/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Simulation: Rate Adaptive Wi-Fi (Minstrel) with moving STA
 * - Two nodes: AP and STA
 * - AP sends UDP CBR traffic at 400 Mbps to STA
 * - STA uses Minstrel rate control
 * - STA moves away from AP in steps; step size and interval configurable
 * - Throughput and distance measured, output to (Gnuplot) file
 * - Optionally log Minstrel rate changes
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/gnuplot.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("MinstrelRateAdaptation");

struct SimMeasurement
{
  double time;
  double distance;
  double throughput;
};

std::vector<SimMeasurement> g_measurements;

static uint64_t lastTotalRx = 0;
static double lastTime = 0.0;

void
CheckThroughputAndDistance (Ptr<Node> ap, Ptr<Node> sta, Ptr<PacketSink> sinkApp)
{
  double curTime = Simulator::Now ().GetSeconds ();
  double distance = sta->GetObject<MobilityModel> ()->GetDistanceFrom (ap->GetObject<MobilityModel> ());
  uint64_t curTotalRx = sinkApp->GetTotalRx ();
  double throughput = (curTotalRx - lastTotalRx) * 8.0 / (curTime - lastTime) / 1e6; // Mbps

  g_measurements.push_back ({curTime, distance, throughput});

  lastTotalRx = curTotalRx;
  lastTime = curTime;

  // Schedule next sample if not at end
  if (Simulator::Now ().GetSeconds () + 1.0 < Simulator::GetMaximumSimulationTime ().GetSeconds ())
    {
      Simulator::Schedule (Seconds (1.0), &CheckThroughputAndDistance, ap, sta, sinkApp);
    }
}

// Optionally log Minstrel events
void
LogMinstrelEvents (std::string context, uint32_t mode, uint32_t dataRate, uint32_t basicRate)
{
  std::cout << Simulator::Now ().GetSeconds () << "s: " << context
            << ": mode=" << mode
            << ", dataRate=" << dataRate << ", basicRate=" << basicRate << std::endl;
}

int
main (int argc, char *argv[])
{
  double simulationTime = 40.0;
  double stepSize = 5.0;         // meters
  double stepInterval = 3.0;     // seconds
  double startingDistance = 5.0; // meters
  bool minstrelLogging = false;
  std::string outputFile = "throughput-vs-distance.plt";

  CommandLine cmd;
  cmd.AddValue ("simulationTime", "Total duration of the simulation (s)", simulationTime);
  cmd.AddValue ("stepSize", "STA movement step size (meters)", stepSize);
  cmd.AddValue ("stepInterval", "STA moves every this interval (s)", stepInterval);
  cmd.AddValue ("startingDistance", "Initial STA-AP distance (meters)", startingDistance);
  cmd.AddValue ("minstrelLogging", "Enable Minstrel rate control logging", minstrelLogging);
  cmd.AddValue ("outputFile", "Gnuplot output filename", outputFile);
  cmd.Parse (argc, argv);

  // Nodes: AP and STA
  NodeContainer wifiStaNode, wifiApNode;
  wifiStaNode.Create (1);
  wifiApNode.Create (1);

  // Wifi PHY/MAC configuration
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());

  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211ac);

  // For AP: Use default rate control (let AP choose best)
  WifiMacHelper mac;
  Ssid ssid = Ssid ("minstrel-network");

  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid),
               "ActiveProbing", BooleanValue (false));
  wifi.SetRemoteStationManager ("ns3::MinstrelWifiManager"); // For STA

  NetDeviceContainer staDevice = wifi.Install (phy, mac, wifiStaNode);

  // AP MAC/Device
  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid));
  // Use default (constant) WifiManager for AP
  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                "DataMode", StringValue ("VhtMcs9"),
                                "ControlMode", StringValue ("VhtMcs0"));

  NetDeviceContainer apDevice = wifi.Install (phy, mac, wifiApNode);

  // Mobility: AP fixed at (0,0,0), STA at (startingDistance,0,0)
  MobilityHelper mobility;
  Ptr<ListPositionAllocator> posAlloc = CreateObject<ListPositionAllocator> ();
  posAlloc->Add (Vector (0.0, 0.0, 0.0));                 // AP
  posAlloc->Add (Vector (startingDistance, 0.0, 0.0));    // STA
  mobility.SetPositionAllocator (posAlloc);
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (wifiApNode);
  mobility.Install (wifiStaNode);

  // Install Internet Stack
  InternetStackHelper stack;
  stack.Install (wifiApNode);
  stack.Install (wifiStaNode);

  // IP address assignment
  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer apInterface = address.Assign (apDevice);
  Ipv4InterfaceContainer staInterface = address.Assign (staDevice);

  // UDP Traffic: AP (sender) to STA (receiver)
  uint16_t port = 9;
  // Packet sink on STA
  Address sinkAddress (InetSocketAddress (staInterface.GetAddress (0), port));
  PacketSinkHelper sinkHelper ("ns3::UdpSocketFactory", sinkAddress);
  ApplicationContainer sinkApps = sinkHelper.Install (wifiStaNode.Get (0));
  sinkApps.Start (Seconds (0.0));
  sinkApps.Stop (Seconds (simulationTime + 1));

  // CBR UDP OnOff Application on AP
  OnOffHelper onoff ("ns3::UdpSocketFactory", sinkAddress);
  onoff.SetAttribute ("DataRate", DataRateValue (DataRate ("400Mbps")));
  onoff.SetAttribute ("PacketSize", UintegerValue (1400));
  onoff.SetAttribute ("StartTime", TimeValue (Seconds (1.0)));
  onoff.SetAttribute ("StopTime", TimeValue (Seconds (simulationTime)));
  ApplicationContainer app = onoff.Install (wifiApNode.Get (0));

  // Schedule STA movement
  Ptr<Node> apNode = wifiApNode.Get (0);
  Ptr<Node> staNode = wifiStaNode.Get (0);
  Ptr<MobilityModel> staMobility = staNode->GetObject<MobilityModel> ();
  Vector position = staMobility->GetPosition ();

  uint32_t numSteps = (uint32_t)((simulationTime - 1.0) / stepInterval);

  for (uint32_t i = 1; i <= numSteps; ++i)
    {
      double when = 1.0 + i * stepInterval;
      double newX = startingDistance + i * stepSize;
      Simulator::Schedule (Seconds (when),
                           [staMobility, newX] {
                             Vector curPos = staMobility->GetPosition ();
                             staMobility->SetPosition (Vector (newX, curPos.y, curPos.z));
                             NS_LOG_INFO ("STA moved to x=" << newX);
                           });
    }

  // Throughput/distance sampling
  lastTotalRx = 0;
  lastTime = 1.0;
  Simulator::Schedule (Seconds (1.0), &CheckThroughputAndDistance, apNode, staNode, DynamicCast<PacketSink> (sinkApps.Get (0)));

  // Enable Minstrel logging if needed
  if (minstrelLogging)
    {
      // Note: Logging available if tracing is enabled in Minstrel
      // Information can be traced from WifiRemoteStationManager or NetDevice
      // For demo, just hook to Wi-Fi PHY rate change events
      Config::Connect ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/RemoteStationManager/RateChange",
                       MakeCallback (&LogMinstrelEvents));
      // For full logging, enable component logs:
      // ns3::MinstrelWifiManager=level_info
      LogComponentEnable ("MinstrelWifiManager", LOG_LEVEL_INFO);
    }

  Simulator::Stop (Seconds (simulationTime + 1));
  Simulator::Run ();
  Simulator::Destroy ();

  // Output: Gnuplot file of throughput vs distance
  Gnuplot plot (outputFile);
  plot.SetTitle ("Throughput vs Distance (Minstrel Rate Control)");
  plot.SetTerminal ("png");
  plot.SetLegend ("Distance (m)", "Throughput (Mbps)");

  Gnuplot2dDataset dataset;
  dataset.SetStyle (Gnuplot2dDataset::LINES_POINTS);

  for (const auto &entry : g_measurements)
    {
      dataset.Add (entry.distance, entry.throughput);
    }
  plot.AddDataset (dataset);

  std::ofstream plotFile (outputFile);
  plot.GenerateOutput (plotFile);
  plotFile.close ();

  std::cout << "Plot data written to " << outputFile << std::endl;

  return 0;
}