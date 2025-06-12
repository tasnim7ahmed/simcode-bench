#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/gnuplot.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiRateAdaptiveMinstrelExample");

class Experiment
{
public:
  Experiment ();
  void Run (double distanceStep, double interval, uint32_t steps, bool enableLog, std::string output);

private:
  void MoveSta ();
  void MonitorThroughput ();
  void LogRate (std::string context, uint32_t newRate);
  void OutputGnuplot ();

  Ptr<Node> apNode;
  Ptr<Node> staNode;
  Ptr<YansWifiChannel> channel;
  Ptr<Socket> srcSocket;
  Ipv4InterfaceContainer interfaces;
  Gnuplot2dDataset dataset;
  std::vector<double> v_distances;
  std::vector<double> v_throughput;
  double distanceStep;
  double interval;
  uint32_t steps;
  uint32_t currentStep;
  bool enableLog;
  std::string output;
  Ptr<PacketSink> sinkApp;
  Time lastCalcTime;
  uint64_t lastRx;
};

Experiment::Experiment () {}

void
Experiment::LogRate (std::string context, uint32_t newRate)
{
  NS_LOG_UNCOND (Simulator::Now ().GetSeconds ()
                 << "s STA phy rate now: " << newRate * 1e-6 << " Mbps");
}

void
Experiment::MonitorThroughput ()
{
  uint64_t totalRx = sinkApp->GetTotalRx ();
  Time now = Simulator::Now ();

  if (currentStep > 0)
    {
      double rx = (totalRx - lastRx) * 8.0 / (now.GetSeconds () - lastCalcTime.GetSeconds ()) / 1e6; // Mbps
      v_throughput.push_back (rx);
    }
  else
    {
      v_throughput.push_back (0.0);
    }
  lastRx = totalRx;
  lastCalcTime = now;
}

void
Experiment::MoveSta ()
{
  if (currentStep >= steps)
    {
      Simulator::Schedule (Seconds (0.5), &Experiment::OutputGnuplot, this);
      Simulator::Stop (Seconds (Simulator::Now ().GetSeconds () + 1));
      return;
    }

  double dist = distanceStep * currentStep;
  // Move STA
  Ptr<MobilityModel> mob = staNode->GetObject<MobilityModel> ();
  mob->SetPosition (Vector (dist, 0.0, 0.0));
  v_distances.push_back (dist);

  // Monitor throughput at this distance (after last interval)
  MonitorThroughput ();

  currentStep++;
  Simulator::Schedule (Seconds (interval), &Experiment::MoveSta, this);
}

void
Experiment::OutputGnuplot ()
{
  std::string fname = output;
  Gnuplot plot (fname.c_str ());
  plot.SetTitle ("STA UDP Throughput vs. Distance");
  plot.SetLegend ("Distance (m)", "Throughput (Mbps)");
  dataset.SetTitle ("Minstrel Throughput");
  dataset.SetStyle (Gnuplot2dDataset::LINES_POINTS);

  for (size_t i = 0; i < v_distances.size () && i < v_throughput.size (); ++i)
    {
      plot.AddDataset (dataset);
      dataset.Add (v_distances[i], v_throughput[i]);
    }
  std::ofstream of (fname);
  plot.GenerateOutput (of);
  of.close ();
}

void
Experiment::Run (double distStep, double intv, uint32_t nsteps, bool log, std::string outfname)
{
  distanceStep = distStep;
  interval = intv;
  steps = nsteps;
  enableLog = log;
  output = outfname;
  currentStep = 0;
  lastCalcTime = Seconds (0.0);
  lastRx = 0;
  v_distances.clear ();
  v_throughput.clear ();
  dataset.Clear ();

  // Nodes
  NodeContainer nodes;
  nodes.Create (2);
  apNode = nodes.Get (0);
  staNode = nodes.Get (1);

  // Wi-Fi
  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211ax_5GHZ);
  wifi.SetRemoteStationManager ("ns3::MinstrelWifiManager", // Note: AP can use any; STA will be forced to Minstrel below
                                "RtsCtsThreshold", UintegerValue (2200));

  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  channel = CreateObject<YansWifiChannel> ();
  YansWifiChannelHelper chan = YansWifiChannelHelper::Default ();
  phy.SetChannel (chan.Create ());

  WifiMacHelper mac;
  Ssid ssid = Ssid ("ssid-rate");

  // AP
  mac.SetType ("ns3::ApWifiMac", "Ssid", SsidValue (ssid));
  NetDeviceContainer apDev;
  apDev = wifi.Install (phy, mac, apNode);

  // STA
  mac.SetType ("ns3::StaWifiMac", "Ssid", SsidValue (ssid), "ActiveProbing", BooleanValue (false));
  NetDeviceContainer staDev = wifi.Install (phy, mac, staNode);

  // Force Minstrel only for STA
  Ptr<WifiNetDevice> staWifiDev = staDev.Get (0)->GetObject<WifiNetDevice> ();
  Ptr<WifiRemoteStationManager> mng = staWifiDev->GetRemoteStationManager ();
  mng->SetAttribute ("RateControlAlgorithm", StringValue ("ns3::MinstrelWifiManager"));

  // Logging, if enabled
  if (enableLog)
    {
      Config::Connect ("/NodeList/1/DeviceList/0/$ns3::WifiNetDevice/Phy/State/RxRateChange", MakeCallback (&Experiment::LogRate, this));
    }

  // Mobility
  MobilityHelper mobility;
  Ptr<ListPositionAllocator> pos = CreateObject<ListPositionAllocator> ();
  pos->Add (Vector (0.0, 0.0, 0.0)); // AP
  pos->Add (Vector (0.0, 0.0, 0.0)); // STA, will move
  mobility.SetPositionAllocator (pos);
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (nodes);

  // Internet
  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  interfaces = ipv4.Assign (NetDeviceContainer (apDev, staDev));

  // UDP Application (CBR)
  uint16_t port = 9;
  // Sink on STA
  PacketSinkHelper sinkHelper ("ns3::UdpSocketFactory", InetSocketAddress (interfaces.GetAddress (1), port));
  ApplicationContainer sinkApps = sinkHelper.Install (staNode);
  sinkApp = StaticCast<PacketSink> (sinkApps.Get (0));
  sinkApps.Start (Seconds (0.0));
  sinkApps.Stop (Seconds (interval * steps + 2));

  // OnOff on AP -> STA
  OnOffHelper onOff ("ns3::UdpSocketFactory", InetSocketAddress (interfaces.GetAddress (1), port));
  onOff.SetConstantRate (DataRate ("400Mbps"), 1400);
  onOff.SetAttribute ("StartTime", TimeValue (Seconds (0.2)));
  onOff.SetAttribute ("StopTime", TimeValue (Seconds (interval * steps + 1)));
  onOff.Install (apNode);

  // Schedule movement and throughput logging
  Simulator::Schedule (Seconds (0.2), &Experiment::MoveSta, this);

  Simulator::Run ();
  Simulator::Destroy ();
}

int
main (int argc, char *argv[])
{
  double distanceStep = 5.0;
  double interval = 2.0;
  uint32_t steps = 15;
  bool enableLog = false;
  std::string output = "wifi-throughput-vs-distance.plt";
  CommandLine cmd;
  cmd.AddValue ("distanceStep", "Distance step (meters)", distanceStep);
  cmd.AddValue ("interval", "Time interval per step (seconds)", interval);
  cmd.AddValue ("steps", "Number of distance steps", steps);
  cmd.AddValue ("log", "Enable rate change logging", enableLog);
  cmd.AddValue ("output", "Output Gnuplot file", output);
  cmd.Parse (argc, argv);

  Experiment exp;
  exp.Run (distanceStep, interval, steps, enableLog, output);

  return 0;
}