#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/gnuplot.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("DualApStaWiFiSim");

class NodeStatistics : public Object
{
public:
  NodeStatistics (Ptr<Node> node, Ptr<NetDevice> device, Ptr<PacketSink> sink)
    : m_node (node), m_device (DynamicCast<WifiNetDevice>(device)), m_sink (sink)
  {
    m_bytesReceived = 0;
    m_totalTxPower = 0;
    m_txSamples = 0;
    m_prevRx = 0;
    m_prevTime = Seconds (0);
    m_stateTime[WifiPhy::IDLE] = 0;
    m_stateTime[WifiPhy::CCA_BUSY] = 0;
    m_stateTime[WifiPhy::TX] = 0;
    m_stateTime[WifiPhy::RX] = 0;

    m_phy = m_device->GetPhy ();
    m_prevState = m_phy->GetState ();
    m_lastStateChangeTime = Simulator::Now ();
    m_stateChange = m_phy->TraceConnectWithoutContext ("State", MakeCallback (&NodeStatistics::PhyStateChanged, this));
    m_txTrace = m_phy->TraceConnectWithoutContext ("TxBegin", MakeCallback (&NodeStatistics::PhyTxBegin, this));

    m_sink->TraceConnectWithoutContext ("Rx", MakeCallback (&NodeStatistics::RxReceived, this));
  }

  void
  RxReceived (Ptr<const Packet> packet, const Address &from)
  {
    m_bytesReceived += packet->GetSize ();
  }

  void
  PhyTxBegin (Ptr<const Packet> packet)
  {
    double txPower = m_phy->GetTxPowerStart ();
    m_totalTxPower += txPower;
    m_txSamples++;
  }

  void
  PhyStateChanged (Time start, Time duration, WifiPhyState previousState, WifiPhyState newState)
  {
    Time now = Simulator::Now ();
    Time elapsed = now - m_lastStateChangeTime;
    m_stateTime[m_prevState] += elapsed.GetSeconds ();
    m_lastStateChangeTime = now;
    m_prevState = newState;
  }

  void
  Update ()
  {
    double interval = 1.0;
    Time now = Simulator::Now ();
    double rxBytes = m_bytesReceived - m_prevRx;
    double throughput = rxBytes * 8 / (interval * 1e6); // Mbps

    double avgTxPower = m_txSamples > 0 ? m_totalTxPower / m_txSamples : 0;

    m_throughputTimestamps.push_back (now.GetSeconds ());
    m_throughputs.push_back (throughput);
    m_avgTxPowers.push_back (avgTxPower);

    m_ccabusyTime.push_back (m_stateTime[WifiPhy::CCA_BUSY]);
    m_idleTime.push_back (m_stateTime[WifiPhy::IDLE]);
    m_txTime.push_back (m_stateTime[WifiPhy::TX]);
    m_rxTime.push_back (m_stateTime[WifiPhy::RX]);

    std::cout << "Node " << m_node->GetId ()
              << " at " << now.GetSeconds () << "s: "
              << "Throughput=" << throughput << " Mbps, "
              << "AvgTxPower=" << avgTxPower << " dBm, "
              << "CCA_BUSY=" << m_stateTime[WifiPhy::CCA_BUSY]
              << "s, IDLE=" << m_stateTime[WifiPhy::IDLE] << "s, "
              << "TX=" << m_stateTime[WifiPhy::TX] << "s, "
              << "RX=" << m_stateTime[WifiPhy::RX] << "s, "
              << "BytesRx=" << m_bytesReceived << std::endl;

    // Reset for next interval
    m_prevRx = m_bytesReceived;
    m_totalTxPower = 0;
    m_txSamples = 0;
    m_stateTime[WifiPhy::CCA_BUSY] = 0;
    m_stateTime[WifiPhy::IDLE] = 0;
    m_stateTime[WifiPhy::TX] = 0;
    m_stateTime[WifiPhy::RX] = 0;

    // Schedule next update
    if (now.GetSeconds () + interval < m_simTime)
      Simulator::Schedule (Seconds (interval), &NodeStatistics::Update, this);
  }

  void
  SetSimTime (double simTime)
  {
    m_simTime = simTime;
  }

  std::vector<double> m_throughputTimestamps;
  std::vector<double> m_throughputs;
  std::vector<double> m_avgTxPowers;
  std::vector<double> m_ccabusyTime, m_idleTime, m_txTime, m_rxTime;

private:
  Ptr<Node> m_node;
  Ptr<WifiNetDevice> m_device;
  Ptr<WifiPhy> m_phy;
  Ptr<PacketSink> m_sink;
  TracedCallback<Time, Time, WifiPhyState, WifiPhyState> m_stateChange;
  TracedCallback<Ptr<const Packet>> m_txTrace;

  uint64_t m_bytesReceived;
  uint64_t m_prevRx;
  double m_totalTxPower;
  uint32_t m_txSamples;
  double m_simTime;

  std::map<WifiPhyState, double> m_stateTime;
  WifiPhyState m_prevState;
  Time m_lastStateChangeTime;
};

void
CreatePositions (Ptr<ListPositionAllocator> alloc,
                 std::vector<Vector> positions)
{
  for (auto &pos : positions)
    {
      alloc->Add (pos);
    }
}

void
ConfigureWiFi (NetDeviceContainer &devices, std::string phyMode,
               std::string wifiManager, double txPowerStart, double txPowerEnd,
               double rtsCtsThr)
{
  Config::SetDefault ("ns3::WifiRemoteStationManager::RtsCtsThreshold",
                      UintegerValue ((uint32_t) rtsCtsThr));
  Config::SetDefault ("ns3::WifiPhy::TxPowerStart", DoubleValue (txPowerStart));
  Config::SetDefault ("ns3::WifiPhy::TxPowerEnd",   DoubleValue (txPowerEnd));
  Config::SetDefault ("ns3::WifiRemoteStationManager::NonUnicastMode",
                      StringValue (phyMode));
  Config::SetDefault ("ns3::WifiRemoteStationManager::FragmentationThreshold", StringValue ("2200"));

  if (wifiManager == "ParfWifiManager")
    Config::SetDefault ("ns3::WifiRemoteStationManager::RtsCtsThreshold", UintegerValue ((uint32_t) rtsCtsThr));
  // wifiManager can be changed dynamically in InstallWifi
}

void
InstallWifi (NodeContainer &staNodes, NodeContainer &apNodes,
             NetDeviceContainer &staDevices, NetDeviceContainer &apDevices,
             std::string phyMode, std::string ssidString,
             std::string wifiManager, double txPowerStart, double txPowerEnd,
             double rtsCtsThr)
{
  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211a);
  wifi.SetRemoteStationManager (wifiManager, "RtsCtsThreshold", UintegerValue ((uint32_t) rtsCtsThr),
                                            "FragmentationThreshold", UintegerValue (2200),
                                            "NonUnicastMode", StringValue (phyMode));
  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  wifiPhy.Set ("TxPowerStart", DoubleValue (txPowerStart));
  wifiPhy.Set ("TxPowerEnd", DoubleValue (txPowerEnd));
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
  wifiPhy.SetChannel (wifiChannel.Create ());
  WifiMacHelper wifiMac;

  // Install AP
  Ssid ssid = Ssid (ssidString);
  wifiMac.SetType ("ns3::ApWifiMac",
                   "Ssid", SsidValue (ssid));
  apDevices = wifi.Install (wifiPhy, wifiMac, apNodes);
  // Install STA
  wifiMac.SetType ("ns3::StaWifiMac",
                   "Ssid", SsidValue (ssid),
                   "ActiveProbing", BooleanValue (false));
  staDevices = wifi.Install (wifiPhy, wifiMac, staNodes);
}

void
PlotStats (std::vector<std::shared_ptr<NodeStatistics>> &stats, std::string prefix)
{
  Gnuplot throughputPlot (prefix + "-throughput.png");
  Gnuplot txPowerPlot (prefix + "-txpower.png");
  Gnuplot ccaPlot (prefix + "-cca.png");
  Gnuplot idlePlot (prefix + "-idle.png");
  Gnuplot txPlot (prefix + "-tx.png");
  Gnuplot rxPlot (prefix + "-rx.png");

  for (size_t i = 0; i < stats.size (); ++i)
    {
      Gnuplot2dDataset thr ("Node" + std::to_string (i));
      Gnuplot2dDataset txp ("Node" + std::to_string (i));
      Gnuplot2dDataset cca ("Node" + std::to_string (i));
      Gnuplot2dDataset idle ("Node" + std::to_string (i));
      Gnuplot2dDataset tx ("Node" + std::to_string (i));
      Gnuplot2dDataset rx ("Node" + std::to_string (i));
      for (size_t j = 0; j < stats[i]->m_throughputs.size (); ++j)
        {
          thr.Add (stats[i]->m_throughputTimestamps[j], stats[i]->m_throughputs[j]);
          txp.Add (stats[i]->m_throughputTimestamps[j], stats[i]->m_avgTxPowers[j]);
          cca.Add (stats[i]->m_throughputTimestamps[j], stats[i]->m_ccabusyTime[j]);
          idle.Add (stats[i]->m_throughputTimestamps[j], stats[i]->m_idleTime[j]);
          tx.Add (stats[i]->m_throughputTimestamps[j], stats[i]->m_txTime[j]);
          rx.Add (stats[i]->m_throughputTimestamps[j], stats[i]->m_rxTime[j]);
        }
      throughputPlot.AddDataset (thr);
      txPowerPlot.AddDataset (txp);
      ccaPlot.AddDataset (cca);
      idlePlot.AddDataset (idle);
      txPlot.AddDataset (tx);
      rxPlot.AddDataset (rx);
    }

  std::ofstream thrFile (prefix + "-throughput.plt");
  std::ofstream txPowFile (prefix + "-txpower.plt");
  std::ofstream ccaFile (prefix + "-cca.plt");
  std::ofstream idleFile (prefix + "-idle.plt");
  std::ofstream txFile (prefix + "-tx.plt");
  std::ofstream rxFile (prefix + "-rx.plt");

  throughputPlot.GenerateOutput (thrFile);
  txPowerPlot.GenerateOutput (txPowFile);
  ccaPlot.GenerateOutput (ccaFile);
  idlePlot.GenerateOutput (idleFile);
  txPlot.GenerateOutput (txFile);
  rxPlot.GenerateOutput (rxFile);

  thrFile.close ();
  txPowFile.close ();
  ccaFile.close ();
  idleFile.close ();
  txFile.close ();
  rxFile.close ();
}

int main (int argc, char *argv[])
{
  double simTime = 100.0;
  double distance = 20.0;

  // Default positions: AP1, AP2, STA1, STA2
  double ap1x = 0.0, ap1y = 0.0, ap1z = 0.0;
  double ap2x = 20.0, ap2y = 0.0, ap2z = 0.0;
  double sta1x = 0.0, sta1y = 10.0, sta1z = 0.0;
  double sta2x = 20.0, sta2y = 10.0, sta2z = 0.0;

  std::string wifiManager = "ns3::ParfWifiManager";
  double rtsCtsThr = 2347;
  double txPowerStart = 16;
  double txPowerEnd = 16;

  CommandLine cmd;
  cmd.AddValue ("simTime", "Simulation time in seconds", simTime);
  cmd.AddValue ("ap1x", "AP1 x position", ap1x);
  cmd.AddValue ("ap1y", "AP1 y position", ap1y);
  cmd.AddValue ("ap1z", "AP1 z position", ap1z);
  cmd.AddValue ("ap2x", "AP2 x position", ap2x);
  cmd.AddValue ("ap2y", "AP2 y position", ap2y);
  cmd.AddValue ("ap2z", "AP2 z position", ap2z);
  cmd.AddValue ("sta1x", "STA1 x position", sta1x);
  cmd.AddValue ("sta1y", "STA1 y position", sta1y);
  cmd.AddValue ("sta1z", "STA1 z position", sta1z);
  cmd.AddValue ("sta2x", "STA2 x position", sta2x);
  cmd.AddValue ("sta2y", "STA2 y position", sta2y);
  cmd.AddValue ("sta2z", "STA2 z position", sta2z);
  cmd.AddValue ("wifiManager", "Wifi manager type", wifiManager);
  cmd.AddValue ("rtsCtsThr", "RTS/CTS threshold", rtsCtsThr);
  cmd.AddValue ("txPowerStart", "Minimum TX Power (dBm)", txPowerStart);
  cmd.AddValue ("txPowerEnd", "Maximum TX Power (dBm)", txPowerEnd);
  cmd.Parse (argc, argv);

  std::vector<Vector> apPositions = { Vector (ap1x, ap1y, ap1z), Vector (ap2x, ap2y, ap2z) };
  std::vector<Vector> staPositions = { Vector (sta1x, sta1y, sta1z), Vector (sta2x, sta2y, sta2z) };

  NodeContainer apNodes, staNodes;
  apNodes.Create (2);
  staNodes.Create (2);

  Ptr<ListPositionAllocator> apAlloc = CreateObject<ListPositionAllocator> ();
  CreatePositions (apAlloc, apPositions);
  Ptr<ListPositionAllocator> staAlloc = CreateObject<ListPositionAllocator> ();
  CreatePositions (staAlloc, staPositions);

  MobilityHelper mobi;
  mobi.SetPositionAllocator (apAlloc);
  mobi.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobi.Install (apNodes);
  mobi.SetPositionAllocator (staAlloc);
  mobi.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobi.Install (staNodes);

  InternetStackHelper stack;
  stack.Install (apNodes);
  stack.Install (staNodes);

  // AP1 <--> STA1 ; AP2 <--> STA2, different SSID
  NetDeviceContainer apDevices1, staDevices1, apDevices2, staDevices2;

  InstallWifi (staNodes.Get (0), apNodes.Get (0),
               staDevices1, apDevices1, "OfdmRate54Mbps",
               "ssid-ap1", wifiManager, txPowerStart, txPowerEnd, rtsCtsThr);

  InstallWifi (staNodes.Get (1), apNodes.Get (1),
               staDevices2, apDevices2, "OfdmRate54Mbps",
               "ssid-ap2", wifiManager, txPowerStart, txPowerEnd, rtsCtsThr);

  // Assign IPs
  Ipv4AddressHelper ipv4a;
  ipv4a.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer ipAp1 = ipv4a.Assign (apDevices1);
  Ipv4InterfaceContainer ipSta1 = ipv4a.Assign (staDevices1);

  ipv4a.SetBase ("10.2.1.0", "255.255.255.0");
  Ipv4InterfaceContainer ipAp2 = ipv4a.Assign (apDevices2);
  Ipv4InterfaceContainer ipSta2 = ipv4a.Assign (staDevices2);

  // Applications
  uint16_t port1 = 5001, port2 = 5002;
  double appStart = 1.0, appStop = simTime;

  // STA1 sink
  Address sink1Addr (InetSocketAddress (ipSta1.GetAddress (0), port1));
  PacketSinkHelper sinkHelper1 ("ns3::UdpSocketFactory", sink1Addr);
  ApplicationContainer sinkApp1 = sinkHelper1.Install (staNodes.Get (0));
  // STA2 sink
  Address sink2Addr (InetSocketAddress (ipSta2.GetAddress (0), port2));
  PacketSinkHelper sinkHelper2 ("ns3::UdpSocketFactory", sink2Addr);
  ApplicationContainer sinkApp2 = sinkHelper2.Install (staNodes.Get (1));

  sinkApp1.Start (Seconds (appStart));
  sinkApp1.Stop (Seconds (appStop));
  sinkApp2.Start (Seconds (appStart));
  sinkApp2.Stop (Seconds (appStop));

  // OnOff on AP1->STA1
  OnOffHelper onoff1 ("ns3::UdpSocketFactory", sink1Addr);
  onoff1.SetAttribute ("DataRate", StringValue ("54Mbps"));
  onoff1.SetAttribute ("PacketSize", UintegerValue (1472));
  ApplicationContainer onoffApp1 = onoff1.Install (apNodes.Get (0));
  onoffApp1.Start (Seconds (appStart));
  onoffApp1.Stop (Seconds (appStop));

  // OnOff on AP2->STA2
  OnOffHelper onoff2 ("ns3::UdpSocketFactory", sink2Addr);
  onoff2.SetAttribute ("DataRate", StringValue ("54Mbps"));
  onoff2.SetAttribute ("PacketSize", UintegerValue (1472));
  ApplicationContainer onoffApp2 = onoff2.Install (apNodes.Get (1));
  onoffApp2.Start (Seconds (appStart));
  onoffApp2.Stop (Seconds (appStop));

  // Node Statistics: only for STA nodes (receivers)
  std::vector<std::shared_ptr<NodeStatistics>> nodeStats;
  auto sink1 = DynamicCast<PacketSink> (sinkApp1.Get (0));
  auto dev1 = staNodes.Get (0)->GetDevice (0);
  auto stat1 = std::make_shared<NodeStatistics> (staNodes.Get (0), dev1, sink1);
  stat1->SetSimTime (simTime);
  nodeStats.push_back (stat1);

  auto sink2 = DynamicCast<PacketSink> (sinkApp2.Get (0));
  auto dev2 = staNodes.Get (1)->GetDevice (0);
  auto stat2 = std::make_shared<NodeStatistics> (staNodes.Get (1), dev2, sink2);
  stat2->SetSimTime (simTime);
  nodeStats.push_back (stat2);

  Simulator::Schedule (Seconds (1.0), &NodeStatistics::Update, stat1.get ());
  Simulator::Schedule (Seconds (1.0), &NodeStatistics::Update, stat2.get ());

  // FlowMonitor
  FlowMonitorHelper flowmonHelper;
  Ptr<FlowMonitor> monitor = flowmonHelper.InstallAll ();

  Simulator::Stop (Seconds (simTime));
  Simulator::Run ();

  // Flowmonitor Stats
  monitor->CheckForLostPackets ();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmonHelper.GetClassifier ());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();

  std::cout << "FlowMonitor results:" << std::endl;
  for (const auto &flow : stats)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (flow.first);
      std::cout << "Flow " << flow.first << " (" << t.sourceAddress << ":" << t.sourcePort
                << " -> " << t.destinationAddress << ":" << t.destinationPort << ")\n";
      std::cout << "  Tx Packets: " << flow.second.txPackets << "\n";
      std::cout << "  Rx Packets: " << flow.second.rxPackets << "\n";
      double rxThr = (flow.second.rxBytes * 8.0) / (flow.second.timeLastRxPacket.GetSeconds () - flow.second.timeFirstTxPacket.GetSeconds ()) / 1e6;
      std::cout << "  Throughput: " << rxThr << " Mbps\n";
      std::cout << "  Mean delay: " << (flow.second.delaySum.GetSeconds () / flow.second.rxPackets) << " s\n";
      std::cout << "  Jitter: " << (flow.second.jitterSum.GetSeconds () / flow.second.rxPackets) << " s\n";
    }

  // Plotting
  PlotStats (nodeStats, "stats");

  Simulator::Destroy ();
  return 0;
}