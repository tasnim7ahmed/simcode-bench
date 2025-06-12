#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/gnuplot.h"
#include <vector>
#include <map>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiMimoThroughput");

class WifiMimoThroughput : public Object
{
public:
  static TypeId GetTypeId (void);
  WifiMimoThroughput ();
  void RunSimulation ();

private:
  void SetupNetwork ();
  void SetupApplications ();
  void CalculateThroughput ();
  void ScheduleNextDistance ();
  void PlotResults ();

  uint32_t m_mcs;
  uint32_t m_streams;
  uint32_t m_numDistances;
  double m_startDistance;
  double m_endDistance;
  double m_distanceStep;
  double m_simTime;
  bool m_useTcp;
  double m_frequency;
  uint16_t m_channelWidth;
  bool m_shortGuardInterval;
  bool m_preambleDetection;
  bool m_channelBonding;
  NodeContainer m_apNode;
  NodeContainer m_staNode;
  NetDeviceContainer m_apDevice;
  NetDeviceContainer m_staDevice;
  Ipv4InterfaceContainer m_apInterface;
  Ipv4InterfaceContainer m_staInterface;
  Gnuplot2dDataset m_datasets[4][5]; // 4 streams x 5 MCS per stream?
  std::map<std::string, double> m_results;
};

TypeId
WifiMimoThroughput::GetTypeId (void)
{
  static TypeId tid = TypeId ("WifiMimoThroughput")
    .SetParent<Object> ()
    .AddConstructor<WifiMimoThroughput> ()
    .AddAttribute ("Mcs", "HT MCS value",
                   UintegerValue (0),
                   MakeUintegerAccessor (&WifiMimoThroughput::m_mcs),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("Streams", "Number of MIMO streams",
                   UintegerValue (1),
                   MakeUintegerAccessor (&WifiMimoThroughput::m_streams),
                   MakeUintegerChecker<uint32_t> (1, 4))
    .AddAttribute ("StartDistance", "Starting distance between AP and STA (m)",
                   DoubleValue (1.0),
                   MakeDoubleAccessor (&WifiMimoThroughput::m_startDistance),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("EndDistance", "Ending distance between AP and STA (m)",
                   DoubleValue (100.0),
                   MakeDoubleAccessor (&WifiMimoThroughput::m_endDistance),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("DistanceStep", "Step size for distance variation (m)",
                   DoubleValue (10.0),
                   MakeDoubleAccessor (&WifiMimoThroughput::m_distanceStep),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("SimTime", "Duration of simulation in seconds",
                   TimeValue (Seconds (10)),
                   MakeTimeAccessor (&WifiMimoThroughput::m_simTime),
                   MakeTimeChecker ())
    .AddAttribute ("UseTcp", "Use TCP instead of UDP traffic",
                   BooleanValue (false),
                   MakeBooleanAccessor (&WifiMimoThroughput::m_useTcp),
                   MakeBooleanChecker ())
    .AddAttribute ("Frequency", "Wi-Fi frequency band: 2.4 or 5.0 GHz",
                   DoubleValue (5.0),
                   MakeDoubleAccessor (&WifiMimoThroughput::m_frequency),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("ChannelWidth", "Channel width in MHz",
                   UintegerValue (20),
                   MakeUintegerAccessor (&WifiMimoThroughput::m_channelWidth),
                   MakeUintegerChecker<uint16_t> ())
    .AddAttribute ("ShortGuardInterval", "Enable short guard interval",
                   BooleanValue (false),
                   MakeBooleanAccessor (&WifiMimoThroughput::m_shortGuardInterval),
                   MakeBooleanChecker ())
    .AddAttribute ("PreambleDetection", "Enable preamble detection",
                   BooleanValue (false),
                   MakeBooleanAccessor (&WifiMimoThroughput::m_preambleDetection),
                   MakeBooleanChecker ())
    .AddAttribute ("ChannelBonding", "Enable channel bonding",
                   BooleanValue (false),
                   MakeBooleanAccessor (&WifiMimoThroughput::m_channelBonding),
                   MakeBooleanChecker ());
  return tid;
}

WifiMimoThroughput::WifiMimoThroughput ()
{
  m_apNode.Create (1);
  m_staNode.Create (1);
}

void
WifiMimoThroughput::RunSimulation ()
{
  SetupNetwork ();
  SetupApplications ();
  Simulator::Stop (m_simTime);
  Simulator::Run ();
  CalculateThroughput ();
  Simulator::Destroy ();
  PlotResults ();
}

void
WifiMimoThroughput::SetupNetwork ()
{
  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211n);

  YansWifiPhyHelper phy;
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  phy.SetChannel (channel.Create ());

  phy.Set ("Frequency", DoubleValue (m_frequency * 1e9));
  phy.Set ("ChannelWidth", UintegerValue (m_channelWidth));
  phy.Set ("ShortGuardIntervalSupported", BooleanValue (m_shortGuardInterval));
  phy.Set ("PreambleDetectionSupported", BooleanValue (m_preambleDetection));

  WifiMacHelper mac;
  Ssid ssid = Ssid ("wifi-mimo");
  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid));
  m_apDevice = wifi.Install (phy, mac, m_apNode);

  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid),
               "ActiveProbing", BooleanValue (false));
  m_staDevice = wifi.Install (phy, mac, m_staNode);

  // Set number of spatial streams
  Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/NumberOfTransmitAntennas", UintegerValue (m_streams));
  Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/NumberOfReceiveAntennas", UintegerValue (m_streams));

  if (m_channelBonding)
    {
      Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/HtConfiguration/ChannelBondingEnabled", BooleanValue (true));
    }

  // Internet stack
  InternetStackHelper stack;
  stack.Install (m_apNode);
  stack.Install (m_staNode);

  Ipv4AddressHelper address;
  address.SetBase ("192.168.1.0", "255.255.255.0");
  m_apInterface = address.Assign (m_apDevice);
  m_staInterface = address.Assign (m_staDevice);

  // Mobility
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

  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (m_startDistance),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (m_distanceStep),
                                 "DeltaY", DoubleValue (0.0),
                                 "GridWidth", UintegerValue (1),
                                 "LayoutType", StringValue ("RowFirst"));
  mobility.Install (m_staNode);
}

void
WifiMimoThroughput::SetupApplications ()
{
  if (m_useTcp)
    {
      BulkSendHelper source ("ns3::TcpSocketFactory",
                             InetSocketAddress (m_apInterface.GetAddress (0), 9));
      ApplicationContainer sourceApps = source.Install (m_staNode.Get (0));
      sourceApps.Start (Seconds (0.0));
      sourceApps.Stop (m_simTime);

      PacketSinkHelper sink ("ns3::TcpSocketFactory",
                             InetSocketAddress (Ipv4Address::GetAny (), 9));
      ApplicationContainer sinkApps = sink.Install (m_apNode.Get (0));
      sinkApps.Start (Seconds (0.0));
      sinkApps.Stop (m_simTime);
    }
  else
    {
      OnOffHelper onoff ("ns3::UdpSocketFactory",
                         Address (InetSocketAddress (m_apInterface.GetAddress (0), 9)));
      onoff.SetConstantRate (DataRate ("100Mbps"));
      ApplicationContainer apps = onoff.Install (m_staNode.Get (0));
      apps.Start (Seconds (0.0));
      apps.Stop (m_simTime);

      PacketSinkHelper sink ("ns3::UdpSocketFactory",
                             InetSocketAddress (Ipv4Address::GetAny (), 9));
      ApplicationContainer sinkApps = sink.Install (m_apNode.Get (0));
      sinkApps.Start (Seconds (0.0));
      sinkApps.Stop (m_simTime);
    }
}

void
WifiMimoThroughput::CalculateThroughput ()
{
  double throughput = 0;
  for (uint32_t i = 0; i < m_staNode.GetN (); ++i)
    {
      Ptr<PacketSink> sink = DynamicCast<PacketSink> (m_apNode.Get (i)->GetApplication (0));
      uint64_t totalBytes = sink->GetTotalRx ();
      throughput = (totalBytes * 8) / (m_simTime.GetSeconds () * 1000000); // Mbps
    }

  double currentDistance = m_startDistance;
  for (uint32_t step = 0; step < m_numDistances; ++step)
    {
      m_datasets[m_streams - 1][m_mcs].Add (currentDistance, throughput);
      currentDistance += m_distanceStep;
    }
}

void
WifiMimoThroughput::ScheduleNextDistance ()
{
  if (m_startDistance <= m_endDistance)
    {
      RunSimulation ();
      m_startDistance += m_distanceStep;
      ScheduleNextDistance ();
    }
}

void
WifiMimoThroughput::PlotResults ()
{
  Gnuplot plot ("throughput-vs-distance.png");
  plot.SetTitle ("Throughput vs Distance for 802.11n MIMO");
  plot.SetXLabel ("Distance (m)");
  plot.SetYLabel ("Throughput (Mbps)");

  for (uint32_t s = 1; s <= 4; ++s)
    {
      for (uint32_t m = 0; m <= 4; ++m)
        {
          std::ostringstream legend;
          legend << s << "-stream MCS-" << m;
          m_datasets[s - 1][m].SetTitle (legend.str ());
          plot.AddDataset (m_datasets[s - 1][m]);
        }
    }

  std::ofstream plotFile ("throughput-vs-distance.plt");
  plot.GenerateOutput (plotFile);
  plotFile.close ();
}

int
main (int argc, char *argv[])
{
  uint32_t mcs = 0;
  uint32_t streams = 1;
  double startDistance = 1.0;
  double endDistance = 100.0;
  double distanceStep = 10.0;
  double simTime = 10.0;
  bool useTcp = false;
  double frequency = 5.0;
  uint16_t channelWidth = 20;
  bool shortGuardInterval = false;
  bool preambleDetection = false;
  bool channelBonding = false;

  CommandLine cmd (__FILE__);
  cmd.AddValue ("mcs", "HT MCS value", mcs);
  cmd.AddValue ("streams", "Number of MIMO streams", streams);
  cmd.AddValue ("startDistance", "Starting distance (m)", startDistance);
  cmd.AddValue ("endDistance", "Ending distance (m)", endDistance);
  cmd.AddValue ("distanceStep", "Distance step (m)", distanceStep);
  cmd.AddValue ("simTime", "Simulation time (s)", simTime);
  cmd.AddValue ("useTcp", "Use TCP traffic", useTcp);
  cmd.AddValue ("frequency", "Wi-Fi frequency (GHz)", frequency);
  cmd.AddValue ("channelWidth", "Channel width (MHz)", channelWidth);
  cmd.AddValue ("shortGuardInterval", "Short guard interval enabled", shortGuardInterval);
  cmd.AddValue ("preambleDetection", "Preamble detection enabled", preambleDetection);
  cmd.AddValue ("channelBonding", "Channel bonding enabled", channelBonding);
  cmd.Parse (argc, argv);

  WifiMimoThroughput simulator;
  simulator.SetAttribute ("Mcs", UintegerValue (mcs));
  simulator.SetAttribute ("Streams", UintegerValue (streams));
  simulator.SetAttribute ("StartDistance", DoubleValue (startDistance));
  simulator.SetAttribute ("EndDistance", DoubleValue (endDistance));
  simulator.SetAttribute ("DistanceStep", DoubleValue (distanceStep));
  simulator.SetAttribute ("SimTime", TimeValue (Seconds (simTime)));
  simulator.SetAttribute ("UseTcp", BooleanValue (useTcp));
  simulator.SetAttribute ("Frequency", DoubleValue (frequency));
  simulator.SetAttribute ("ChannelWidth", UintegerValue (channelWidth));
  simulator.SetAttribute ("ShortGuardInterval", BooleanValue (shortGuardInterval));
  simulator.SetAttribute ("PreambleDetection", BooleanValue (preambleDetection));
  simulator.SetAttribute ("ChannelBonding", BooleanValue (channelBonding));

  simulator.RunSimulation ();

  return 0;
}