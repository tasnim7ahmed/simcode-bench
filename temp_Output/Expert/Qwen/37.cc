#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"
#include "ns3/gnuplot.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiRateControlComparison");

class WifiSimulation
{
public:
  WifiSimulation ();
  void Setup (std::string rateManager, uint32_t rtsThreshold, std::string throughputFile, std::string powerFile);
  void Run ();

private:
  void ScheduleDistanceChange ();
  void LogStats ();
  void UpdatePosition ();

  NodeContainer m_apNode;
  NodeContainer m_staNode;
  NetDeviceContainer m_apDev;
  NetDeviceContainer m_staDev;
  Ipv4InterfaceContainer m_apInterface;
  Ipv4InterfaceContainer m_staInterface;
  UdpServerHelper m_serverHelper;
  UdpClientHelper m_clientHelper;
  ApplicationContainer m_serverApp;
  ApplicationContainer m_clientApp;
  Gnuplot2dDataset m_throughputDataset;
  Gnuplot2dDataset m_powerDataset;
  double m_distance;
  double m_currentTime;
  double m_interval;
  uint32_t m_packetsSent;
};

WifiSimulation::WifiSimulation ()
  : m_distance (1.0),
    m_currentTime (0.0),
    m_interval (1.0),
    m_packetsSent (0)
{
}

void
WifiSimulation::Setup (std::string rateManager, uint32_t rtsThreshold, std::string throughputFile, std::string powerFile)
{
  // Create nodes
  m_apNode.Create (1);
  m_staNode.Create (1);

  // Setup mobility
  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (0.0),
                                 "DeltaY", DoubleValue (0.0),
                                 "GridWidth", UintegerValue (1),
                                 "LayoutType", StringValue ("RowFirst"));
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (m_apNode);

  mobility.SetMobilityModel ("ns3::ConstantVelocityMobilityModel");
  mobility.Install (m_staNode);
  Ptr<ConstantVelocityMobilityModel> staMobility = m_staNode.Get (0)->GetObject<ConstantVelocityMobilityModel> ();
  staMobility->SetVelocity (Vector (1.0, 0.0, 0.0)); // Move along X-axis

  // Setup WiFi
  WifiMacHelper wifiMac;
  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211a);
  wifi.SetRemoteStationManager (rateManager, "RtsCtsThreshold", UintegerValue (rtsThreshold));

  wifiMac.SetType ("ns3::ApWifiMac");
  m_apDev = wifi.Install (wifiPhyHelper (), wifiMac, m_apNode);

  wifiMac.SetType ("ns3::StaWifiMac", "Ssid", SsidValue (Ssid ("wifi-test")));
  m_staDev = wifi.Install (wifiPhyHelper (), wifiMac, m_staNode);

  // Internet stack
  InternetStackHelper stack;
  stack.Install (m_apNode);
  stack.Install (m_staNode);

  Ipv4AddressHelper address;
  address.SetBase ("192.168.1.0", "255.255.255.0");
  m_apInterface = address.Assign (m_apDev);
  m_staInterface = address.Assign (m_staDev);

  // Traffic
  m_serverHelper = UdpServerHelper (9);
  m_serverApp = m_serverHelper.Install (m_staNode.Get (0));
  m_serverApp.Start (Seconds (0.0));
  m_serverApp.Stop (Seconds (20.0));

  m_clientHelper = UdpClientHelper (m_staInterface.GetAddress (0), 9);
  m_clientHelper.SetAttribute ("MaxPackets", UintegerValue (4294967295u));
  m_clientHelper.SetAttribute ("Interval", TimeValue (Seconds (0.001)));
  m_clientHelper.SetAttribute ("PacketSize", UintegerValue (1472));
  m_clientApp = m_clientHelper.Install (m_apNode.Get (0));
  m_clientApp.Start (Seconds (1.0));
  m_clientApp.Stop (Seconds (20.0));

  // Setup gnuplot datasets
  m_throughputDataset.SetTitle ("Throughput vs Distance");
  m_throughputDataset.SetStyle (Gnuplot2dDataset::LINES_POINTS);
  m_powerDataset.SetTitle ("Tx Power vs Distance");
  m_powerDataset.SetStyle (Gnuplot2dDataset::LINES_POINTS);

  Gnuplot throughputPlot = Gnuplot (throughputFile);
  throughputPlot.AddDataset (m_throughputDataset);
  throughputPlot.SetTerminal ("png");
  throughputPlot.SetLegend ("Distance (m)", "Throughput (Mbps)");
  throughputPlot.SetExtra ("set grid");

  Gnuplot powerPlot = Gnuplot (powerFile);
  powerPlot.AddDataset (m_powerDataset);
  powerPlot.SetTerminal ("png");
  powerPlot.SetLegend ("Distance (m)", "Average Tx Power (dBm)");
  powerPlot.SetExtra ("set grid");

  Config::Connect ("/NodeList/*/DeviceList/*/Phy/TxTrace", MakeCallback (&LogStats, this));
}

void
WifiSimulation::Run ()
{
  Simulator::Stop (Seconds (20.0));
  ScheduleDistanceChange ();
  Simulator::Run ();
  Simulator::Destroy ();
}

void
WifiSimulation::ScheduleDistanceChange ()
{
  if (m_currentTime < 20.0)
    {
      Simulator::Schedule (Seconds (m_currentTime), &UpdatePosition, this);
      m_currentTime += 1.0;
      ScheduleDistanceChange ();
    }
}

void
WifiSimulation::UpdatePosition ()
{
  Vector position = m_staNode.Get (0)->GetObject<MobilityModel> ()->GetPosition ();
  m_distance = position.x;
  NS_LOG_UNCOND ("STA moved to distance: " << m_distance << " meters");
}

void
WifiSimulation::LogStats ()
{
  double now = Simulator::Now ().GetSeconds ();
  double throughput = (m_packetsSent * 1472.0 * 8) / (1e6 * 1.0); // Mbps over last second
  m_packetsSent = 0;

  double avgPower = 0.0;
  for (auto dev : m_staDev)
    {
      avgPower += DynamicCast<WifiNetDevice> (dev)->GetPhy ()->GetTxPowerLevel ();
    }
  avgPower /= m_staDev.GetN ();

  m_throughputDataset.Add (m_distance, throughput);
  m_powerDataset.Add (m_distance, avgPower);
}

int
main (int argc, char *argv[])
{
  std::string rateManager = "ns3::ParfWifiManager";
  uint32_t rtsThreshold = 2346;
  std::string throughputFile = "throughput.png";
  std::string powerFile = "txpower.png";

  CommandLine cmd (__FILE__);
  cmd.AddValue ("rateManager", "WiFi rate control manager", rateManager);
  cmd.AddValue ("rtsThreshold", "RTS threshold", rtsThreshold);
  cmd.AddValue ("throughputFile", "Output file for throughput plot", throughputFile);
  cmd.AddValue ("powerFile", "Output file for tx power plot", powerFile);
  cmd.Parse (argc, argv);

  WifiSimulation sim;
  sim.Setup (rateManager, rtsThreshold, throughputFile, powerFile);
  sim.Run ();

  return 0;
}