#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/gnuplot.h"
#include "ns3/stats-module.h"
#include <fstream>
#include <iomanip>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiSimulation");

class NodeStatistics : public Object
{
public:
  static TypeId GetTypeId ();
  NodeStatistics ();
  ~NodeStatistics () override;

  void SetNode (Ptr<Node> node);
  void SetStartTime (Time startTime);
  void SetEndTime (Time endTime);

  void Initialize ();
  void Finalize ();
  void PrintStatistics ();
  void SchedulePrintStatistics ();

private:
  void RxEvent (Ptr<const Packet> packet, uint16_t channelFreqMhz, WifiBand wifiBand, uint16_t phyMode);
  void TxEvent (Ptr<const Packet> packet, uint16_t channelFreqMhz, WifiBand wifiBand, uint16_t phyMode);
  void CcaBusyEvent (Time duration);
  void IdleEvent (Time duration);

  Ptr<Node> m_node;
  int m_nodeId;
  Time m_startTime;
  Time m_endTime;

  Time m_lastPrintTime;

  Time m_totalCcaBusyTime;
  Time m_totalIdleTime;
  Time m_totalTxTime;
  Time m_totalRxTime;

  uint64_t m_totalBytesReceived;
  double m_averageTransmitPower;
  uint32_t m_txPackets;

  Address m_sinkAddress;
  uint16_t m_sinkPort;

  DataRate m_lastRxRate;

  //Gnuplot
  Gnuplot2dDataset m_throughputDataset;
  Gnuplot2dDataset m_txPowerDataset;
  Gnuplot2dDataset m_ccaBusyDataset;
  Gnuplot2dDataset m_idleDataset;
  Gnuplot2dDataset m_txDataset;
  Gnuplot2dDataset m_rxDataset;

  Gnuplot m_gnuplot;
};

TypeId
NodeStatistics::GetTypeId ()
{
  static TypeId tid = TypeId ("NodeStatistics")
    .SetParent<Object> ()
    .SetGroupName("Wifi");
  return tid;
}

NodeStatistics::NodeStatistics ()
  : m_nodeId (-1),
    m_totalCcaBusyTime (Seconds (0.0)),
    m_totalIdleTime (Seconds (0.0)),
    m_totalTxTime (Seconds (0.0)),
    m_totalRxTime (Seconds (0.0)),
    m_totalBytesReceived (0),
    m_averageTransmitPower (0.0),
    m_txPackets (0)
{
  m_sinkPort = 9;
}

NodeStatistics::~NodeStatistics ()
{
}

void
NodeStatistics::SetNode (Ptr<Node> node)
{
  m_node = node;
  m_nodeId = m_node->GetId ();
}

void
NodeStatistics::SetStartTime (Time startTime)
{
  m_startTime = startTime;
}

void
NodeStatistics::SetEndTime (Time endTime)
{
  m_endTime = endTime;
}

void
NodeStatistics::Initialize ()
{
  Config::Connect ("/NodeList/" + std::to_string (m_nodeId) + "/DeviceList/*/Phy/MonitorSnifferRx", MakeCallback (&NodeStatistics::RxEvent, this));
  Config::Connect ("/NodeList/" + std::to_string (m_nodeId) + "/DeviceList/*/Phy/MonitorSnifferTx", MakeCallback (&NodeStatistics::TxEvent, this));
  Config::Connect ("/NodeList/" + std::to_string (m_nodeId) + "/DeviceList/*/WifiNetDevice/Phy/State/StateVariables/ccaBusyTime", MakeCallback (&NodeStatistics::CcaBusyEvent, this));
  Config::Connect ("/NodeList/" + std::to_string (m_nodeId) + "/DeviceList/*/WifiNetDevice/Phy/State/StateVariables/idleTime", MakeCallback (&NodeStatistics::IdleEvent, this));

  // Setup PacketSink to track received bytes
  Ptr<Ipv4> ipv4 = m_node->GetObject<Ipv4> ();
  Ipv4Address sinkAddress = ipv4->GetAddress (1, 0).GetLocal ();
  m_sinkAddress = Address (sinkAddress);

  PacketSinkHelper packetSinkHelper ("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), m_sinkPort));
  ApplicationContainer sinkApps = packetSinkHelper.Install (m_node);
  sinkApps.Start (m_startTime);
  sinkApps.Stop (m_endTime);

  m_lastPrintTime = m_startTime;

  //Gnuplot
  std::string fileNameWithNoExtension = "node-" + std::to_string (m_nodeId);

  m_throughputDataset.SetTitle ("Throughput");
  m_throughputDataset.SetStyle (Gnuplot2dDataset::LINES_POINTS);

  m_txPowerDataset.SetTitle ("Average Transmit Power");
  m_txPowerDataset.SetStyle (Gnuplot2dDataset::LINES_POINTS);

  m_ccaBusyDataset.SetTitle ("CCA Busy Time");
  m_ccaBusyDataset.SetStyle (Gnuplot2dDataset::LINES_POINTS);

  m_idleDataset.SetTitle ("Idle Time");
  m_idleDataset.SetStyle (Gnuplot2dDataset::LINES_POINTS);

  m_txDataset.SetTitle ("Tx Time");
  m_txDataset.SetStyle (Gnuplot2dDataset::LINES_POINTS);

  m_rxDataset.SetTitle ("Rx Time");
  m_rxDataset.SetStyle (Gnuplot2dDataset::LINES_POINTS);

  m_gnuplot.AddDataset (m_throughputDataset);
  m_gnuplot.AddDataset (m_txPowerDataset);
  m_gnuplot.AddDataset (m_ccaBusyDataset);
  m_gnuplot.AddDataset (m_idleDataset);
  m_gnuplot.AddDataset (m_txDataset);
  m_gnuplot.AddDataset (m_rxDataset);

  m_gnuplot.SetTerminal ("png");
  m_gnuplot.SetOutput (fileNameWithNoExtension + ".png");
  m_gnuplot.SetTitle ("Node " + std::to_string (m_nodeId) + " Statistics");
  m_gnuplot.SetLegend ("Time (s)", "Value");

  SchedulePrintStatistics ();
}

void
NodeStatistics::Finalize ()
{
  PrintStatistics ();
  m_gnuplot.GenerateOutput ();
}

void
NodeStatistics::PrintStatistics ()
{
  Time now = Simulator::Now ();
  Time deltaT = now - m_lastPrintTime;

  uint64_t bytesReceived = DynamicCast<PacketSink> (m_node->GetApplication (0))->GetTotalRx ();
  double throughput = (double)(bytesReceived - m_totalBytesReceived) * 8 / deltaT.ToDouble (Time::S);
  double avgTxPower = (m_txPackets > 0) ? m_averageTransmitPower / m_txPackets : 0.0;

  NS_LOG_UNCOND ("Node " << m_nodeId << " Statistics at " << now.ToDouble (Time::S) << "s:");
  NS_LOG_UNCOND ("  Throughput: " << throughput << " bps");
  NS_LOG_UNCOND ("  Average Transmit Power: " << avgTxPower << " dBm");
  NS_LOG_UNCOND ("  CCA Busy Time: " << m_totalCcaBusyTime.ToDouble (Time::S) << " s");
  NS_LOG_UNCOND ("  Idle Time: " << m_totalIdleTime.ToDouble (Time::S) << " s");
  NS_LOG_UNCOND ("  Tx Time: " << m_totalTxTime.ToDouble (Time::S) << " s");
  NS_LOG_UNCOND ("  Rx Time: " << m_totalRxTime.ToDouble (Time::S) << " s");

  m_throughputDataset.Add (now.ToDouble (Time::S), throughput);
  m_txPowerDataset.Add (now.ToDouble (Time::S), avgTxPower);
  m_ccaBusyDataset.Add (now.ToDouble (Time::S), m_totalCcaBusyTime.ToDouble (Time::S));
  m_idleDataset.Add (now.ToDouble (Time::S), m_totalIdleTime.ToDouble (Time::S));
  m_txDataset.Add (now.ToDouble (Time::S), m_totalTxTime.ToDouble (Time::S));
  m_rxDataset.Add (now.ToDouble (Time::S), m_totalRxTime.ToDouble (Time::S));

  m_totalBytesReceived = bytesReceived;
  m_averageTransmitPower = 0.0;
  m_txPackets = 0;

  m_lastPrintTime = now;
}

void
NodeStatistics::SchedulePrintStatistics ()
{
  if (Simulator::Now () < m_endTime)
    {
      Simulator::Schedule (Seconds (1.0), &NodeStatistics::PrintStatistics, this);
      Simulator::Schedule (Seconds (1.0), &NodeStatistics::SchedulePrintStatistics, this);
    }
  else
    {
      Finalize ();
    }
}

void
NodeStatistics::RxEvent (Ptr<const Packet> packet, uint16_t channelFreqMhz, WifiBand wifiBand, uint16_t phyMode)
{
  m_totalRxTime += Simulator::Now () - m_lastPrintTime;
}

void
NodeStatistics::TxEvent (Ptr<const Packet> packet, uint16_t channelFreqMhz, WifiBand wifiBand, uint16_t phyMode)
{
  m_totalTxTime += Simulator::Now () - m_lastPrintTime;
  WifiMacHeader hdr;
  packet->PeekHeader (hdr);
  double txPowerDbm = 0.0;
  if (packet->GetDistanceFromLastTx () >= 0)
    {
      txPowerDbm = packet->GetLastTxPowerDbm ();
    }
  m_averageTransmitPower += txPowerDbm;
  m_txPackets++;
}

void
NodeStatistics::CcaBusyEvent (Time duration)
{
  m_totalCcaBusyTime += duration;
}

void
NodeStatistics::IdleEvent (Time duration)
{
  m_totalIdleTime += duration;
}

int
main (int argc, char *argv[])
{
  bool verbose = false;
  double simulationTime = 100;
  std::string wifiManagerType = "ns3::ParfWifiManager";
  uint32_t rtsCtsThreshold = 2347;
  int64_t defaultTxPower = 16;
  double staApDistance = 5.0;

  CommandLine cmd;
  cmd.AddValue ("verbose", "Tell application to log if true.", verbose);
  cmd.AddValue ("simulationTime", "Simulation time in seconds.", simulationTime);
  cmd.AddValue ("wifiManager", "Wifi Manager Type.", wifiManagerType);
  cmd.AddValue ("rtsCtsThreshold", "RTS/CTS Threshold", rtsCtsThreshold);
  cmd.AddValue ("txPower", "Transmit Power in dBm", defaultTxPower);
  cmd.AddValue ("distance", "Distance between AP and STA", staApDistance);
  cmd.Parse (argc, argv);

  if (verbose)
    {
      LogComponentEnable ("WifiSimulation", LOG_LEVEL_INFO);
    }

  Config::SetDefault ("ns3::WifiMacQueue::MaxSize", StringValue ("1000000"));

  NodeContainer apNodes;
  apNodes.Create (2);

  NodeContainer staNodes;
  staNodes.Create (2);

  // Configure mobility for APs and STAs
  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  positionAlloc->Add (Vector (0.0, 0.0, 0.0)); // AP1
  positionAlloc->Add (Vector (10.0, 0.0, 0.0)); // AP2
  positionAlloc->Add (Vector (0.0 + staApDistance, 5.0, 0.0)); // STA1
  positionAlloc->Add (Vector (10.0 + staApDistance, 5.0, 0.0)); // STA2
  mobility.SetPositionAllocator (positionAlloc);
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (apNodes);
  mobility.Install (staNodes);

  // Configure the PHY and channel
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());
  phy.Set ("TxPowerStart", DoubleValue (defaultTxPower));
  phy.Set ("TxPowerEnd", DoubleValue (defaultTxPower));

  // Configure the MAC
  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211a);
  wifi.SetRemoteStationManager (wifiManagerType);

  WifiMacHelper mac;
  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (Ssid ("ns3-wifi")),
               "BeaconInterval", TimeValue (Seconds (0.075)));

  NetDeviceContainer apDevices = wifi.Install (phy, mac, apNodes);

  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (Ssid ("ns3-wifi")),
               "ActiveProbing", BooleanValue (false),
               "RtsCtsThreshold", UintegerValue (rtsCtsThreshold));

  NetDeviceContainer staDevices = wifi.Install (phy, mac, staNodes);

  // Set different SSIDs for each AP-STA pair
  Ssid ssid1 = Ssid ("AP1-STA1");
  Ssid ssid2 = Ssid ("AP2-STA2");

  Config::Set ("/NodeList/0/DeviceList/*/$ns3::WifiNetDevice/Mac/Ssid", SsidValue (ssid1));
  Config::Set ("/NodeList/2/DeviceList/*/$ns3::WifiNetDevice/Mac/Ssid", SsidValue (ssid1));

  Config::Set ("/NodeList/1/DeviceList/*/$ns3::WifiNetDevice/Mac/Ssid", SsidValue (ssid2));
  Config::Set ("/NodeList/3/DeviceList/*/$ns3::WifiNetDevice/Mac/Ssid", SsidValue (ssid2));

  // Install the internet stack
  InternetStackHelper stack;
  stack.Install (apNodes);
  stack.Install (staNodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer apInterfaces = address.Assign (apDevices);
  Ipv4InterfaceContainer staInterfaces = address.Assign (staDevices);

  // Configure CBR traffic from AP to STA
  uint16_t port = 9;
  OnOffHelper onoff ("ns3::UdpSocketFactory",
                     Address (InetSocketAddress (staInterfaces.GetAddress (0), port)));
  onoff.SetConstantRate (DataRate ("54Mbps"));
  ApplicationContainer clientApps1 = onoff.Install (apNodes.Get (0));
  clientApps1.Start (Seconds (1.0));
  clientApps1.Stop (Seconds (simulationTime + 1));

  OnOffHelper onoff2 ("ns3::UdpSocketFactory",
                     Address (InetSocketAddress (staInterfaces.GetAddress (1), port)));
  onoff2.SetConstantRate (DataRate ("54Mbps"));
  ApplicationContainer clientApps2 = onoff2.Install (apNodes.Get (1));
  clientApps2.Start (Seconds (1.0));
  clientApps2.Stop (Seconds (simulationTime + 1));

  // Create NodeStatistics objects for STAs
  Ptr<NodeStatistics> staStats1 = CreateObject<NodeStatistics> ();
  staStats1->SetNode (staNodes.Get (0));
  staStats1->SetStartTime (Seconds (0.0));
  staStats1->SetEndTime (Seconds (simulationTime));
  staStats1->Initialize ();

  Ptr<NodeStatistics> staStats2 = CreateObject<NodeStatistics> ();
  staStats2->SetNode (staNodes.Get (1));
  staStats2->SetStartTime (Seconds (0.0));
  staStats2->SetEndTime (Seconds (simulationTime));
  staStats2->Initialize ();

  // Create FlowMonitor
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  // Run the simulation
  Simulator::Stop (Seconds (simulationTime));
  Simulator::Run ();

  // Print Flow Monitor statistics
  monitor->CheckForLostPackets ();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();
  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
      NS_LOG_UNCOND ("Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")");
      NS_LOG_UNCOND ("  Tx Bytes:   " << i->second.txBytes);
      NS_LOG_UNCOND ("  Rx Bytes:   " << i->second.rxBytes);
      NS_LOG_UNCOND ("  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds () - i->second.timeFirstTxPacket.GetSeconds ()) / 1024 / 1024 << " Mbps");
      NS_LOG_UNCOND ("  Delay:      " << i->second.delaySum.GetSeconds () / i->second.rxPackets * 1000 << " ms");
      NS_LOG_UNCOND ("  Jitter:     " << i->second.jitterSum.GetSeconds () / (i->second.rxPackets - 1) * 1000 << " ms");
    }

  monitor->SerializeToXmlFile ("wifi-simulation.flowmon", true, true);

  Simulator::Destroy ();
  return 0;
}