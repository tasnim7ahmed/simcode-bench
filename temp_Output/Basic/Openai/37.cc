#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/gnuplot.h"
#include <fstream>
#include <string>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiRateControlComparison");

class SimulationTracer
{
public:
  SimulationTracer ();
  void TxPowerTrace (double oldPower, double newPower);
  void RateTrace (DataRate oldRate, DataRate newRate);
  void RecordThroughput (double throughput);
  void RecordTxPower (double txPower);
  void OutputResults (std::string throughputFilename, std::string txpowerFilename);

  std::vector<double> m_distances;
  std::vector<double> m_throughputs;
  std::vector<double> m_txPowers;
};

SimulationTracer::SimulationTracer ()
{
}

void
SimulationTracer::TxPowerTrace (double oldPower, double newPower)
{
  m_txPowers.push_back (newPower);
}

void
SimulationTracer::RateTrace (DataRate oldRate, DataRate newRate)
{
  // Optional: Could log rate changes if needed
}

void
SimulationTracer::RecordThroughput (double throughput)
{
  m_throughputs.push_back (throughput);
}

void
SimulationTracer::RecordTxPower (double txPower)
{
  m_txPowers.push_back (txPower);
}

void
SimulationTracer::OutputResults (std::string throughputFilename, std::string txpowerFilename)
{
  std::ofstream thrf (throughputFilename);
  for (size_t i = 0; i < m_distances.size (); ++i)
    {
      thrf << m_distances[i] << " " << m_throughputs[i] << std::endl;
    }
  thrf.close ();

  std::ofstream pwrf (txpowerFilename);
  for (size_t i = 0; i < m_distances.size (); ++i)
    {
      pwrf << m_distances[i] << " " << m_txPowers[i] << std::endl;
    }
  pwrf.close ();
}

void
AdvanceSta (Ptr<MobilityModel> staMobility, double step, double &distance, SimulationTracer *tracer, Ptr<WifiPhy> phy, Ptr<Node> apNode, Time interval)
{
  Vector pos = staMobility->GetPosition ();
  pos.x += step;
  staMobility->SetPosition (pos);
  distance += step;
  tracer->m_distances.push_back (distance);

  // Record current transmit power (AP)
  tracer->RecordTxPower (phy->GetTxPowerStart ());
}

class ThroughputSampler
{
public:
  ThroughputSampler ();

  void RxPacket (Ptr<const Packet> packet, const Address &addr);
  void Sample (SimulationTracer *tracer, double interval, uint32_t &oldTotalRx, double &lastSampleTime, double curDistance);

  uint32_t m_totalRx;
};

ThroughputSampler::ThroughputSampler ()
  : m_totalRx (0)
{
}

void
ThroughputSampler::RxPacket (Ptr<const Packet> packet, const Address &addr)
{
  m_totalRx += packet->GetSize ();
}

void
ThroughputSampler::Sample (SimulationTracer *tracer, double interval, uint32_t &oldTotalRx, double &lastSampleTime, double curDistance)
{
  double throughput = ((m_totalRx - oldTotalRx) * 8.0) / (interval * 1e6); // Mbps
  tracer->RecordThroughput (throughput);
  oldTotalRx = m_totalRx;
  lastSampleTime += interval;
}

int
main (int argc, char *argv[])
{
  std::string manager = "ParfWifiManager";
  double rtsThreshold = 2347;
  std::string throughputFile = "throughput.dat";
  std::string txpowerFile = "txpower.dat";
  double distanceStep = 1.0;
  double distanceMax = 100.0;
  double stepInterval = 1.0; // s

  CommandLine cmd;
  cmd.AddValue ("manager", "WiFi rate control manager: ParfWifiManager|AparfWifiManager|RrpaaWifiManager", manager);
  cmd.AddValue ("rtsThreshold", "RTS/CTS threshold (bytes)", rtsThreshold);
  cmd.AddValue ("throughputFile", "Output throughput filename", throughputFile);
  cmd.AddValue ("txpowerFile", "Output txpower filename", txpowerFile);
  cmd.Parse (argc, argv);

  uint32_t nSta = 1;
  uint32_t nAp = 1;

  NodeContainer wifiStaNode;
  wifiStaNode.Create (nSta);
  NodeContainer wifiApNode;
  wifiApNode.Create (nAp);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());

  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211g);

  if (manager == "ParfWifiManager")
    wifi.SetRemoteStationManager ("ns3::ParfWifiManager", "RtsCtsThreshold", UintegerValue ((uint32_t) rtsThreshold));
  else if (manager == "AparfWifiManager")
    wifi.SetRemoteStationManager ("ns3::AparfWifiManager", "RtsCtsThreshold", UintegerValue ((uint32_t) rtsThreshold));
  else if (manager == "RrpaaWifiManager")
    wifi.SetRemoteStationManager ("ns3::RrpaaWifiManager", "RtsCtsThreshold", UintegerValue ((uint32_t) rtsThreshold));
  else
    {
      std::cerr << "Invalid manager name\n";
      return 1;
    }

  WifiMacHelper mac;
  Ssid ssid = Ssid ("ns3-wifi");

  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid),
               "ActiveProbing", BooleanValue (false));
  NetDeviceContainer staDevice;
  staDevice = wifi.Install (phy, mac, wifiStaNode);

  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid));
  NetDeviceContainer apDevice;
  apDevice = wifi.Install (phy, mac, wifiApNode);

  MobilityHelper mobility;
  // AP at (0,0,0)
  Ptr<ListPositionAllocator> posAlloc = CreateObject<ListPositionAllocator> ();
  posAlloc->Add (Vector (0.0, 0.0, 0.0));
  mobility.SetPositionAllocator (posAlloc);
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (wifiApNode);

  Ptr<ListPositionAllocator> staPosAlloc = CreateObject<ListPositionAllocator> ();
  staPosAlloc->Add (Vector (0.0, 0.0, 0.0));
  mobility.SetPositionAllocator (staPosAlloc);
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (wifiStaNode);

  InternetStackHelper stack;
  stack.Install (wifiApNode);
  stack.Install (wifiStaNode);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer staInterfaces, apInterfaces;
  staInterfaces = address.Assign (staDevice);
  apInterfaces = address.Assign (apDevice);

  // Application: AP sends UDP to STA
  uint16_t port = 5001;
  OnOffHelper onoff ("ns3::UdpSocketFactory", InetSocketAddress (staInterfaces.GetAddress (0), port));
  onoff.SetAttribute ("DataRate", DataRateValue (DataRate ("54Mbps")));
  onoff.SetAttribute ("PacketSize", UintegerValue (1472));
  onoff.SetAttribute ("StartTime", TimeValue (Seconds (1.0)));
  onoff.SetAttribute ("StopTime", TimeValue (Seconds (distanceMax/stepInterval + 2)));

  ApplicationContainer app = onoff.Install (wifiApNode.Get (0));

  // Packet sink on STA
  PacketSinkHelper sink ("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), port));
  ApplicationContainer sinkApp = sink.Install (wifiStaNode.Get (0));
  sinkApp.Start (Seconds (0.0));
  sinkApp.Stop (Seconds (distanceMax/stepInterval + 3));

  // Tracing and logging
  SimulationTracer tracer;
  ThroughputSampler sampler;
  Ptr<PacketSink> pktSink = DynamicCast<PacketSink> (sinkApp.Get (0));

  pktSink->TraceConnectWithoutContext ("Rx", MakeCallback (&ThroughputSampler::RxPacket, &sampler));

  // Connect PHY/MAC power/rate traces on AP
  Ptr<WifiNetDevice> apWifiNetDev = apDevice.Get (0)->GetObject<WifiNetDevice> ();
  Ptr<WifiPhy> apPhy = apWifiNetDev->GetPhy ();

  // Distance movement setup
  Ptr<MobilityModel> staMobility = wifiStaNode.Get (0)->GetObject<MobilityModel> ();
  double curDistance = 0.0;
  uint32_t oldTotalRx = 0;
  double lastSampleTime = 0.0;
  double simTime = distanceMax / distanceStep * stepInterval + 3.0;

  for (double t = 1.0; t <= distanceMax / distanceStep; ++t)
    {
      Simulator::Schedule (Seconds (t * stepInterval),
          &AdvanceSta, staMobility, distanceStep, std::ref (curDistance), &tracer, apPhy, wifiApNode.Get(0), Seconds (stepInterval));
      Simulator::Schedule (Seconds (t * stepInterval),
          &ThroughputSampler::Sample, &sampler, &tracer, stepInterval, std::ref (oldTotalRx), std::ref (lastSampleTime), curDistance);
    }

  // Initial sample at t=0
  tracer.m_distances.push_back (0.0);
  tracer.m_txPowers.push_back (apPhy->GetTxPowerStart ());
  tracer.m_throughputs.push_back (0.0);

  Simulator::Stop (Seconds (simTime));
  Simulator::Run ();

  tracer.OutputResults (throughputFile, txpowerFile);

  Simulator::Destroy ();
  return 0;
}