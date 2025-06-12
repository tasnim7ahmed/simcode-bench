#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/stats-module.h"
#include "ns3/gnuplot.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiMinstrelRateAdaptSimulation");

class ThroughputVsDistance
{
public:
  ThroughputVsDistance ();
  void Run ();

private:
  void ScheduleMovement ();
  void MoveSta ();
  void CheckThroughput ();
  void LogCurrentRate ();

  uint32_t m_distanceSteps;
  double m_distanceStep;
  double m_interval;
  std::string m_phyMode;
  bool m_enableRateLogging;
  std::string m_outputFile;

  NodeContainer m_nodes;
  NetDeviceContainer m_devices;
  Ptr<Socket> m_sinkSocket;
  uint64_t m_lastTotalRx;
  Gnuplot2dDataset m_throughputDataset;
  std::vector<double> m_distances;
  std::vector<double> m_tputHistory;
  uint32_t m_stepCount;
  Ptr<MobilityModel> m_staMobility;
};

ThroughputVsDistance::ThroughputVsDistance ()
  : m_distanceSteps (10),
    m_distanceStep (5.0),
    m_interval (2.0),
    m_phyMode ("HtMcs7"),
    m_enableRateLogging (false),
    m_outputFile ("throughput-vs-distance.plt"),
    m_lastTotalRx (0),
    m_stepCount (0)
{
}

void
ThroughputVsDistance::Run ()
{
  CommandLine cmd;
  cmd.AddValue ("distanceSteps", "Number of steps for STA movement", m_distanceSteps);
  cmd.AddValue ("distanceStep", "Distance step (meters)", m_distanceStep);
  cmd.AddValue ("interval", "Time interval per step (seconds)", m_interval);
  cmd.AddValue ("phyMode", "Wifi Phy mode", m_phyMode);
  cmd.AddValue ("enableRateLogging", "Enable logging of rate changes", m_enableRateLogging);
  cmd.AddValue ("outputFile", "Gnuplot output filename", m_outputFile);
  cmd.Parse ();


  m_nodes.Create (2); // Node 0: AP, Node 1: STA

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());

  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211n_5GHZ);
  wifi.SetRemoteStationManager ("ns3::MinstrelWifiManager",
                                "RtsCtsThreshold", UintegerValue (99999)); // Default for all stations

  WifiMacHelper mac;
  Ssid ssid = Ssid ("ns3-ssid");

  // AP
  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid));
  NetDeviceContainer apDevice = wifi.Install (phy, mac, m_nodes.Get (0));

  // STA
  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid),
               "ActiveProbing", BooleanValue (false));
  NetDeviceContainer staDevice = wifi.Install (phy, mac, m_nodes.Get (1));

  m_devices.Add (apDevice);
  m_devices.Add (staDevice);

  // Set Minstrel only for STA
  Ptr<NetDevice> staDev = staDevice.Get (0);
  Ptr<WifiNetDevice> staWifiDev = DynamicCast<WifiNetDevice> (staDev);
  staWifiDev->SetRemoteStationManager ("ns3::MinstrelWifiManager");

  // Mobility: AP fixed, STA moves
  MobilityHelper mobility;
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (m_nodes);

  Ptr<MobilityModel> apMobility = m_nodes.Get (0)->GetObject<MobilityModel> ();
  apMobility->SetPosition (Vector (0.0, 0.0, 0.0));
  m_staMobility = m_nodes.Get (1)->GetObject<MobilityModel> ();
  m_staMobility->SetPosition (Vector (0.0, 0.0, 0.0));

  // Internet stack
  InternetStackHelper internet;
  internet.Install (m_nodes);
  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = ipv4.Assign (m_devices);

  // UDP traffic: AP sends to STA
  uint16_t port = 9;
  OnOffHelper onoff ("ns3::UdpSocketFactory", InetSocketAddress (interfaces.GetAddress (1), port));
  onoff.SetAttribute ("DataRate", DataRateValue (DataRate ("400Mbps")));
  onoff.SetAttribute ("PacketSize", UintegerValue (1472));
  onoff.SetAttribute ("StartTime", TimeValue (Seconds (0.5)));
  onoff.SetAttribute ("StopTime", TimeValue (Seconds ((m_distanceSteps + 1) * m_interval)));
  ApplicationContainer app = onoff.Install (m_nodes.Get (0));

  PacketSinkHelper sink ("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), port));
  ApplicationContainer sinkApp = sink.Install (m_nodes.Get (1));
  sinkApp.Start (Seconds (0.0));
  sinkApp.Stop (Seconds ((m_distanceSteps + 1) * m_interval));

  m_sinkSocket = DynamicCast<PacketSink> (sinkApp.Get (0))->GetSocket ();

  // Enable tracing if enabled
  if (m_enableRateLogging)
    {
      phy.EnablePcap ("wifi-minstrel", m_devices, true, true);

      Config::ConnectWithoutContext ("/NodeList/1/DeviceList/0/$ns3::WifiNetDevice/Phy/State/Tx",
                                    MakeCallback ([&](Ptr<const Packet> packet, WifiTxVector txVector, WifiPreamble preamble, WifiMode mode, WifiTxVector, Time, uint16_t){
      NS_LOG_INFO ("[Time " << Simulator::Now ().GetSeconds() <<
                   "s] STA rate: " << txVector.GetMode ().GetDataRate (phy.GetChannelWidth ()) / 1e6 << " Mbps, Mode: " << txVector.GetMode ().GetUniqueName ());
      }));
    }

  m_throughputDataset.SetTitle ("STA throughput vs distance");
  m_throughputDataset.SetStyle (Gnuplot2dDataset::LINES_POINTS);

  m_stepCount = 0;
  m_lastTotalRx = 0;
  Simulator::Schedule (Seconds (m_interval), &ThroughputVsDistance::ScheduleMovement, this);

  Simulator::Stop (Seconds ((m_distanceSteps + 1) * m_interval + 1));
  Simulator::Run ();
  Simulator::Destroy ();

  // Output Gnuplot file
  Gnuplot plot (m_outputFile);
  plot.SetTitle ("Station Throughput vs Distance with Minstrel");
  plot.SetTerminal ("png");
  plot.SetLegend ("Distance (m)", "Throughput (Mbps)");
  plot.AddDataset (m_throughputDataset);
  std::ofstream ofs (m_outputFile);
  plot.GenerateOutput (ofs);
  ofs.close ();
}

void
ThroughputVsDistance::ScheduleMovement ()
{
  MoveSta ();
  Simulator::Schedule (Seconds (m_interval - 0.2),
    &ThroughputVsDistance::CheckThroughput, this);
  ++m_stepCount;
  if (m_stepCount < m_distanceSteps)
    {
      Simulator::Schedule (Seconds (0.2), &ThroughputVsDistance::ScheduleMovement, this);
    }
}
void
ThroughputVsDistance::MoveSta ()
{
  Vector pos = m_staMobility->GetPosition ();
  pos.x += m_distanceStep;
  m_staMobility->SetPosition (pos);
  m_distances.push_back (pos.x);
}
void
ThroughputVsDistance::CheckThroughput ()
{
  Ptr<PacketSink> sink = DynamicCast<PacketSink> (m_sinkSocket->GetNode ()->GetApplication (0));
  uint64_t totalRx = sink->GetTotalRx ();
  double throughput = (totalRx - m_lastTotalRx) * 8.0 / (m_interval * 1e6); // Mbps
  m_lastTotalRx = totalRx;
  double currentDistance = m_staMobility->GetPosition ().x;
  m_throughputDataset.Add (currentDistance, throughput);
}

int
main (int argc, char *argv[])
{
  ThroughputVsDistance sim;
  sim.Run ();
  return 0;
}