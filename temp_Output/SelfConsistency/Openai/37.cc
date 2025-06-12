/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/**
 * Compare WiFi Rate Control (ARF-family): Parf, Aparf, Rrpaa in a basic scenario.
 * - 2 nodes: AP and STA
 * - AP sends UDP CBR at 54 Mbps to STA
 * - STA moves away 1 m every second
 * - Log changes in Tx power and rate
 * - Output Gnuplot-compatible data files for throughput and TxPower vs distance
 * - Command-line parameters for: 
 *    --manager: rate control mechanism [Parf, Aparf, Rrpaa]
 *    --rtsThreshold: RTS/CTS threshold
 *    --throughputFile: output file for throughput data
 *    --txPowerFile: output file for transmit power data
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/gnuplot.h"
#include <fstream>
#include <iomanip>
#include <string>
#include <map>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiRateControlComparison");

// Global logs for throughput and tx power
struct TimeSample {
  double time;
  double distance;
  double throughput;   // bits/sec
  double txPower;      // dBm
};

std::vector<TimeSample> g_samples;

std::map< uint64_t, double > g_txPowerMap;
std::map< uint64_t, std::string > g_dataRateMap;

void
TxPhyTrace(std::string context, Ptr<const Packet> packet, WifiMode mode, WifiPreamble preamble, uint8_t txPower)
{
  // context: /NodeList/0/DeviceList/0/$ns3::WifiNetDevice/Phy/TxPhyTrace
  // Here, we can note txPower used for each packet
  // We use UID to identify packets
  g_txPowerMap[packet->GetUid()] = static_cast<double>(txPower);
  g_dataRateMap[packet->GetUid()] = mode.GetModulationClass() == WIFI_MOD_CLASS_HT ? mode.GetUniqueName() : mode.GetDataRate(); // For info
}

void
LogTxPowerRateChange(Ptr<WifiPhy> phy, double oldValue, double newValue)
{
  NS_LOG_INFO ("TxPower changed to " << newValue << " dBm at " << Simulator::Now().GetSeconds() << "s");
}

void
LogDataRateChange(std::string context, uint64_t oldRate, uint64_t newRate)
{
  NS_LOG_INFO ("DataRate changed to " << newRate << " bps at " << Simulator::Now().GetSeconds() << "s");
}

// Function to schedule node movement and stats logging
void
MoveAndLogStep(Ptr<MobilityModel> staMobility,
               Ptr<Node> apNode,
               Ptr<Node> staNode,
               double stepDistance,
               double simulationTime,
               uint32_t step,
               uint32_t totalSteps,
               Ptr<PacketSink> sink,
               std::vector<double> *txPowers)
{
  double distance = step * stepDistance;
  staMobility->SetPosition (Vector (distance, 0.0, 0.0));
  double now = Simulator::Now().GetSeconds();

  // Calculate throughput for preceding interval
  static uint64_t lastTotalRx = 0;
  uint64_t curTotalRx = sink->GetTotalRx ();
  double throughput = 0.0;
  if (now > 0)
    throughput = 8.0 * (curTotalRx - lastTotalRx) / 1.0; // bps over last 1 second

  lastTotalRx = curTotalRx;

  // Get AP's Phy TxPower (assume device 0)
  Ptr<WifiNetDevice> wifiDev = DynamicCast<WifiNetDevice>(apNode->GetDevice (0));
  double txPower = 0.0;
  if (!txPowers->empty())
    txPower = txPowers->back();

  g_samples.push_back ({now, distance, throughput, txPower});

  // Schedule next
  if (step + 1 < totalSteps) {
    Simulator::Schedule (Seconds(1.0),
      &MoveAndLogStep, staMobility, apNode, staNode, stepDistance,
      simulationTime, step+1, totalSteps, sink, txPowers);
  }
}

// Trace callback for TxPower change logging (for AP)
void
PhyTxPowerChangeTrace(double oldValue, double newValue)
{
  static std::vector<double> txPowers;
  txPowers.push_back (newValue);
}

int
main (int argc, char *argv[])
{
  std::string manager = "Parf";
  uint32_t rtsThreshold = 2200;
  std::string throughputFile = "throughput.dat";
  std::string txPowerFile = "txpower.dat";
  double simulationTime = 30.0;
  double stepDistance = 1.0;
  double apTxPowerDbm = 16.0206; // 40 mW (default yans, ~16 dBm)
  double staTxPowerDbm = 16.0206;

  CommandLine cmd;
  cmd.AddValue ("manager", "Rate control mechanism: Parf, Aparf, or Rrpaa", manager);
  cmd.AddValue ("rtsThreshold", "RTS threshold (bytes)", rtsThreshold);
  cmd.AddValue ("throughputFile", "Output file for throughput-vs-distance", throughputFile);
  cmd.AddValue ("txPowerFile", "Output file for transmit-power-vs-distance", txPowerFile);
  cmd.Parse (argc, argv);

  if (manager != "Parf" && manager != "Aparf" && manager != "Rrpaa")
    {
      std::cerr << "Unsupported manager: " << manager << "\n";
      return 1;
    }

  // WiFi setup
  NodeContainer nodes;
  nodes.Create (2); // AP (0), STA (1)

  // PHY, channel, MAC
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());
  phy.Set ("TxPowerStart", DoubleValue (apTxPowerDbm));
  phy.Set ("TxPowerEnd", DoubleValue (apTxPowerDbm));
  phy.Set ("TxPowerLevels", UintegerValue (1));

  WifiHelper wifi;
  StringValue managerValue;
  if (manager == "Parf")
    {
      managerValue = StringValue ("ns3::ParfWifiManager");
    }
  else if (manager == "Aparf")
    {
      managerValue = StringValue ("ns3::AparfWifiManager");
    }
  else if (manager == "Rrpaa")
    {
      managerValue = StringValue ("ns3::RrpaaWifiManager");
    }
  wifi.SetRemoteStationManager (managerValue,
    "RtsCtsThreshold", UintegerValue (rtsThreshold));

  // Set standard and channel
  wifi.SetStandard (WIFI_PHY_STANDARD_80211g);

  // MAC helpers
  Ssid ssid = Ssid ("rate-control-test");

  WifiMacHelper mac;
  NetDeviceContainer devices;

  // AP
  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid));
  Ptr<NetDevice> apDev = wifi.Install (phy, mac, nodes.Get(0)).Get (0);

  // STA
  phy.Set ("TxPowerStart", DoubleValue (staTxPowerDbm));
  phy.Set ("TxPowerEnd", DoubleValue (staTxPowerDbm));
  phy.Set ("TxPowerLevels", UintegerValue (1));
  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid));
  Ptr<NetDevice> staDev = wifi.Install (phy, mac, nodes.Get(1)).Get (0);

  devices.Add (apDev);
  devices.Add (staDev);

  // Internet stack
  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("192.168.1.0", "255.255.255.0");
  Ipv4InterfaceContainer ifs = address.Assign (devices);

  // Mobility
  MobilityHelper mobility;

  // AP fixed at origin
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (nodes.Get (0));
  Ptr<MobilityModel> apMob = nodes.Get (0)->GetObject<MobilityModel> ();

  // STA initially at 0m, moves away in steps
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (nodes.Get (1));
  Ptr<MobilityModel> staMob = nodes.Get (1)->GetObject<MobilityModel> ();
  staMob->SetPosition (Vector (0.0, 0.0, 0.0));

  // UDP traffic: AP (0) -> STA (1)
  uint16_t port = 5000;
  OnOffHelper onoff ("ns3::UdpSocketFactory",
                     Address (InetSocketAddress (ifs.GetAddress (1), port)));
  onoff.SetAttribute ("DataRate", DataRateValue (DataRate ("54Mbps")));
  onoff.SetAttribute ("PacketSize", UintegerValue (1472));
  onoff.SetAttribute ("StartTime", TimeValue (Seconds (0.5)));
  onoff.SetAttribute ("StopTime", TimeValue (Seconds (simulationTime)));

  ApplicationContainer app = onoff.Install (nodes.Get (0));
  // Corresponding Sink
  PacketSinkHelper sinkHelper ("ns3::UdpSocketFactory",
                               Address (InetSocketAddress (Ipv4Address::GetAny (), port)));
  ApplicationContainer sinkApp = sinkHelper.Install (nodes.Get (1));
  Ptr<PacketSink> sink = DynamicCast<PacketSink> (sinkApp.Get (0));
  sinkApp.Start (Seconds (0.0));
  sinkApp.Stop (Seconds (simulationTime) + Seconds (1.0));

  // Tracing for TxPower and DataRate changes (AP device)
  Ptr<WifiNetDevice> apWifiDev = DynamicCast<WifiNetDevice> (apDev);
  Ptr<WifiPhy> apPhy = apWifiDev->GetPhy ();
  apPhy->TraceConnectWithoutContext ("TxPower", MakeCallback (&LogTxPowerRateChange));
  // Can't easily trace data rate here, requires REM changes.

  // Start move/log scheduler
  uint32_t totalSteps = static_cast<uint32_t>(simulationTime);
  std::vector<double> txPowerHistory;
  Simulator::Schedule (Seconds(0.0),
    &MoveAndLogStep, staMob, nodes.Get(0), nodes.Get(1),
    stepDistance, simulationTime, 0, totalSteps, sink, &txPowerHistory);

  // Start simulation
  Simulator::Stop (Seconds (simulationTime) + Seconds(1.0));
  Simulator::Run ();
  Simulator::Destroy ();

  // Write Gnuplot-compatible output
  // Throughput
  std::ofstream thrOut (throughputFile.c_str (), std::ios_base::trunc);
  thrOut << "#Distance(m)  Throughput(bps)\n";
  for (const auto &sample : g_samples)
    {
      thrOut << std::fixed << std::setprecision (2)
             << sample.distance << "  " << std::setprecision (0)
             << sample.throughput << "\n";
    }
  thrOut.close ();

  // TxPower (here, txPower is fixed, but for wifis that adapt txpower, would log sample)
  std::ofstream txpOut (txPowerFile.c_str (), std::ios_base::trunc);
  txpOut << "#Distance(m)  TxPower(dBm)\n";
  for (const auto &sample : g_samples)
    {
      txpOut << std::fixed << std::setprecision (2)
             << sample.distance << "  " << sample.txPower << "\n";
    }
  txpOut.close ();

  NS_LOG_INFO ("Simulation complete. Data written to " << throughputFile << " and " << txPowerFile);

  return 0;
}