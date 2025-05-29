#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/gnuplot.h"
#include <vector>
#include <string>
#include <map>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("DualApStaWifiStatsGnuplot");

class NodeStatistics : public Object
{
public:
  NodeStatistics (Ptr<Node> node, Ptr<WifiNetDevice> dev) : m_node(node), m_dev(dev)
  {
    m_bytesReceived = 0;
    m_prevEnergy = 0;
    m_samples = 0;
    m_totalTxPower = 0;
    m_txStateTime = 0;
    m_rxStateTime = 0;
    m_idleStateTime = 0;
    m_ccaBusyStateTime = 0;
    m_lastUpdate = Simulator::Now ();
    m_phy = m_dev->GetPhy ();
    // Connect state time counters
    m_txState = DynamicCast<WifiPhyStateHelper> (m_phy->GetObject<WifiPhy> ())->GetTxState ();
    m_rxState = DynamicCast<WifiPhyStateHelper> (m_phy->GetObject<WifiPhy> ())->GetRxState ();
    m_idleState = DynamicCast<WifiPhyStateHelper> (m_phy->GetObject<WifiPhy> ())->GetIdleState ();
    m_ccaBusyState = DynamicCast<WifiPhyStateHelper> (m_phy->GetObject<WifiPhy> ())->GetCcaBusyState ();

    // Trace wifi phy state changes
    m_phy->TraceConnectWithoutContext ("State", MakeCallback (&NodeStatistics::PhyStateTrace, this));
    m_phy->TraceConnectWithoutContext ("TxPowerStart", MakeCallback (&NodeStatistics::TxPowerTrace, this));
  }

  void SetPacketSinkApp (Ptr<Application> sink)
  {
    m_sink = sink;
    m_sink->TraceConnectWithoutContext ("Rx", MakeCallback (&NodeStatistics::PacketRx, this));
  }

  void PacketRx (Ptr<const Packet> p, const Address &address)
  {
    m_bytesReceived += p->GetSize ();
  }

  void PhyStateTrace (Time start, Time duration, WifiPhyState state)
  {
    Time now = Simulator::Now ();
    Time elapsed = now - m_lastUpdate;
    switch(m_lastState)
      {
      case WifiPhyState::IDLE: m_idleStateTime += elapsed.GetSeconds (); break;
      case WifiPhyState::CCA_BUSY: m_ccaBusyStateTime += elapsed.GetSeconds (); break;
      case WifiPhyState::TX: m_txStateTime += elapsed.GetSeconds (); break;
      case WifiPhyState::RX: m_rxStateTime += elapsed.GetSeconds (); break;
      default: break;
      }
    m_lastUpdate = now;
    m_lastState = state;
  }

  void TxPowerTrace (double txPowerDbm)
  {
    m_totalTxPower += txPowerDbm;
    m_samples++;
  }

  void AdvanceTime ()
  {
    // flush the last state
    PhyStateTrace (Simulator::Now (), Seconds (0), m_lastState);
  }

  uint64_t GetBytesReceived () const
  {
    return m_bytesReceived;
  }

  double GetThroughputMbps (double intervalSec)
  {
    return (m_bytesReceived * 8.0) / (intervalSec * 1e6);
  }

  double GetAvgTxPower () const
  {
    if (m_samples == 0) return 0.0;
    return m_totalTxPower / (double) m_samples;
  }

  double GetStateTime (WifiPhyState state) const
  {
    switch (state)
      {
      case WifiPhyState::IDLE: return m_idleStateTime;
      case WifiPhyState::CCA_BUSY: return m_ccaBusyStateTime;
      case WifiPhyState::TX: return m_txStateTime;
      case WifiPhyState::RX: return m_rxStateTime;
      default: return 0;
      }
  }

  void ResetCounters ()
  {
    m_bytesReceived = 0;
    m_txStateTime = 0;
    m_rxStateTime = 0;
    m_idleStateTime = 0;
    m_ccaBusyStateTime = 0;
    m_samples = 0;
    m_totalTxPower = 0;
    m_lastUpdate = Simulator::Now ();
  }

private:
  Ptr<Node> m_node;
  Ptr<WifiNetDevice> m_dev;
  Ptr<Application> m_sink;
  Ptr<WifiPhy> m_phy;
  uint64_t m_bytesReceived;
  uint64_t m_prevEnergy;
  uint64_t m_samples;
  double m_totalTxPower;
  double m_txStateTime;
  double m_rxStateTime;
  double m_idleStateTime;
  double m_ccaBusyStateTime;
  Time m_lastUpdate;
  WifiPhyState m_lastState = WifiPhyState::IDLE;
  uint64_t m_txState, m_rxState, m_idleState, m_ccaBusyState;
};

struct ApStaPair
{
  Ptr<Node> ap;
  Ptr<Node> sta;
  Ptr<WifiNetDevice> apDev;
  Ptr<WifiNetDevice> staDev;
  Ptr<Application> sinkApp;
  Ptr<Application> senderApp;
  Ptr<NodeStatistics> stat;
  std::string ssid;
};

static void
PrintStatsPerPair (
    std::vector<ApStaPair> & pairs, double interval, std::ofstream & thptStream,
    std::ofstream & txpowStream, std::ofstream & stateStream)
{
  for (size_t i = 0; i < pairs.size (); ++i)
    {
      pairs[i].stat->AdvanceTime ();
      double thpt = pairs[i].stat->GetThroughputMbps (interval);
      double txpwr = pairs[i].stat->GetAvgTxPower ();
      double txT   = pairs[i].stat->GetStateTime (WifiPhyState::TX);
      double rxT   = pairs[i].stat->GetStateTime (WifiPhyState::RX);
      double idleT = pairs[i].stat->GetStateTime (WifiPhyState::IDLE);
      double ccaT  = pairs[i].stat->GetStateTime (WifiPhyState::CCA_BUSY);

      thptStream  << Simulator::Now ().GetSeconds () << " " << thpt << std::endl;
      txpowStream << Simulator::Now ().GetSeconds () << " " << txpwr << std::endl;
      stateStream << Simulator::Now ().GetSeconds ()
                  << " " << txT
                  << " " << rxT
                  << " " << idleT
                  << " " << ccaT << std::endl;
      pairs[i].stat->ResetCounters ();
    }
}

static void
PeriodicStats (
    std::vector<ApStaPair> & pairs, double interval,
    std::ofstream & thptStream, std::ofstream & txpowStream, std::ofstream & stateStream)
{
  PrintStatsPerPair (pairs, interval, thptStream, txpowStream, stateStream);
  if (Simulator::Now () + Seconds (interval) < Seconds (Simulator::GetMaximumSimulationTime ().GetSeconds ()))
    {
      Simulator::Schedule (Seconds (interval),
          &PeriodicStats,
          std::ref (pairs),
          interval,
          std::ref (thptStream),
          std::ref (txpowStream),
          std::ref (stateStream));
    }
}

static void
SetupGnuplots (Gnuplot & thptPlot,
               Gnuplot & txpowPlot,
               Gnuplot & statePlot,
               const std::string & thptFile,
               const std::string & txpowFile,
               const std::string & stateFile)
{
  thptPlot.SetTitle("Per-STA Throughput over Time");
  thptPlot.SetLegend("Time (s)", "Throughput (Mbps)");
  Gnuplot2dDataset thptDataSet;
  thptDataSet.SetTitle("STA Throughput");
  thptDataSet.SetStyle (Gnuplot2dDataset::LINES);
  thptDataSet.SetFile(thptFile);
  thptPlot.AddDataset(thptDataSet);

  txpowPlot.SetTitle("Avg Transmit Power over Time");
  txpowPlot.SetLegend("Time (s)", "Tx Power (dBm)");
  Gnuplot2dDataset txPowDataSet;
  txPowDataSet.SetTitle ("STA Tx Power");
  txPowDataSet.SetStyle (Gnuplot2dDataset::LINES);
  txPowDataSet.SetFile(txpowFile);
  txpowPlot.AddDataset (txPowDataSet);

  statePlot.SetTitle("WiFi Phy State Times");
  statePlot.SetLegend("Time (s)", "Seconds per state");
  Gnuplot2dDataset txState, rxState, idleState, ccaBusyState;
  txState.SetTitle("TX");
  rxState.SetTitle("RX");
  idleState.SetTitle("IDLE");
  ccaBusyState.SetTitle("CCA_BUSY");
  txState.SetStyle (Gnuplot2dDataset::LINES);
  rxState.SetStyle (Gnuplot2dDataset::LINES);
  idleState.SetStyle (Gnuplot2dDataset::LINES);
  ccaBusyState.SetStyle (Gnuplot2dDataset::LINES);
  txState.SetFile(stateFile, 1, 2); // column 2
  rxState.SetFile(stateFile, 1, 3);
  idleState.SetFile(stateFile, 1, 4);
  ccaBusyState.SetFile(stateFile, 1, 5);
  statePlot.AddDataset (txState);
  statePlot.AddDataset (rxState);
  statePlot.AddDataset (idleState);
  statePlot.AddDataset (ccaBusyState);
}

int main (int argc, char *argv[])
{
  uint32_t simulationTime = 100;
  std::string wifiManager = "ns3::ParfWifiManager";
  uint32_t rtsThreshold = 2347;
  double txPowerStart = 16.0206;
  double txPowerEnd = 16.0206;
  uint32_t nApStaPairs = 2;

  // default positions
  std::vector<double> apX = {0, 50};
  std::vector<double> apY = {0, 0};
  std::vector<double> staX = {5, 55};
  std::vector<double> staY = {0, 0};

  CommandLine cmd;
  cmd.AddValue ("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.AddValue ("wifiManager", "Wifi Rate/ARF manager (e.g., ns3::MinstrelWifiManager)", wifiManager);
  cmd.AddValue ("rtsThreshold", "RTS/CTS threshold (bytes)", rtsThreshold);
  cmd.AddValue ("txPowerStart", "Min Tx Power dBm", txPowerStart);
  cmd.AddValue ("txPowerEnd", "Max Tx Power dBm", txPowerEnd);
  cmd.AddValue ("ap0x", "AP0 X position", apX[0]);
  cmd.AddValue ("ap0y", "AP0 Y position", apY[0]);
  cmd.AddValue ("ap1x", "AP1 X position", apX[1]);
  cmd.AddValue ("ap1y", "AP1 Y position", apY[1]);
  cmd.AddValue ("sta0x", "STA0 X position", staX[0]);
  cmd.AddValue ("sta0y", "STA0 Y position", staY[0]);
  cmd.AddValue ("sta1x", "STA1 X position", staX[1]);
  cmd.AddValue ("sta1y", "STA1 Y position", staY[1]);
  cmd.Parse (argc, argv);

  apX[0] = cmd.GetValue<double> ("ap0x", apX[0]);
  apY[0] = cmd.GetValue<double> ("ap0y", apY[0]);
  apX[1] = cmd.GetValue<double> ("ap1x", apX[1]);
  apY[1] = cmd.GetValue<double> ("ap1y", apY[1]);
  staX[0] = cmd.GetValue<double> ("sta0x", staX[0]);
  staY[0] = cmd.GetValue<double> ("sta0y", staY[0]);
  staX[1] = cmd.GetValue<double> ("sta1x", staX[1]);
  staY[1] = cmd.GetValue<double> ("sta1y", staY[1]);

  NodeContainer aps, stas;
  for (uint32_t i = 0; i < nApStaPairs; ++i) aps.Create (1);
  for (uint32_t i = 0; i < nApStaPairs; ++i) stas.Create (1);

  // Position allocator
  Ptr<ListPositionAllocator> posAlloc = CreateObject<ListPositionAllocator> ();
  posAlloc->Add (Vector (apX[0], apY[0], 0));
  posAlloc->Add (Vector (apX[1], apY[1], 0));
  posAlloc->Add (Vector (staX[0], staY[0], 0));
  posAlloc->Add (Vector (staX[1], staY[1], 0));

  MobilityHelper mobility;
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.SetPositionAllocator (posAlloc);

  NodeContainer allNodes;
  allNodes.Add (aps);
  allNodes.Add (stas);
  mobility.Install (allNodes);

  // WiFi
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());
  phy.Set ("TxPowerStart", DoubleValue (txPowerStart));
  phy.Set ("TxPowerEnd",   DoubleValue (txPowerEnd));

  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211a);
  wifi.SetRemoteStationManager (wifiManager, "RtsCtsThreshold", UintegerValue (rtsThreshold));

  // Per-pair helpers
  std::vector<ApStaPair> pairs;
  for (uint32_t i = 0; i < nApStaPairs; ++i)
    {
      Ssid ssid = Ssid ("ap" + std::to_string (i) + "-ssid");
      WifiMacHelper mac;
      mac.SetType ("ns3::ApWifiMac",
                   "Ssid", SsidValue (ssid));
      NetDeviceContainer apDev = wifi.Install (phy, mac, aps.Get (i));

      mac.SetType ("ns3::StaWifiMac",
                   "Ssid", SsidValue (ssid),
                   "ActiveProbing", BooleanValue (false));
      NetDeviceContainer staDev = wifi.Install (phy, mac, stas.Get (i));

      ApStaPair pair;
      pair.ap = aps.Get (i);
      pair.sta = stas.Get (i);
      pair.apDev = DynamicCast<WifiNetDevice> (apDev.Get (0));
      pair.staDev = DynamicCast<WifiNetDevice> (staDev.Get (0));
      pair.ssid = ssid;
      pairs.push_back (pair);
    }

  // Internet stack, assign IPs
  InternetStackHelper stack;
  stack.Install (allNodes);

  Ipv4AddressHelper ipv4;
  std::vector<Ipv4InterfaceContainer> apIfs, staIfs;
  for (uint32_t i = 0; i < nApStaPairs; ++i)
    {
      std::string netstr = "10." + std::to_string (i+1) + ".1.0";
      ipv4.SetBase (Ipv4Address (netstr.c_str ()), "255.255.255.0");
      Ipv4InterfaceContainer apif = ipv4.Assign (NetDeviceContainer (pairs[i].apDev));
      Ipv4InterfaceContainer staif = ipv4.Assign (NetDeviceContainer (pairs[i].staDev));
      apIfs.push_back (apif);
      staIfs.push_back (staif);
    }

  // App layer: OnOff (AP->STA), PacketSink (STA)
  uint16_t basePort = 5000;
  for (uint32_t i = 0; i < nApStaPairs; ++i)
    {
      Address sinkAddr (InetSocketAddress (staIfs[i].GetAddress (0), basePort + i));
      PacketSinkHelper sink ("ns3::UdpSocketFactory", sinkAddr);
      ApplicationContainer sinkApp = sink.Install (pairs[i].sta);
      sinkApp.Start (Seconds (0.0));
      sinkApp.Stop (Seconds (simulationTime));
      pairs[i].sinkApp = sinkApp.Get (0);

      OnOffHelper onOff ("ns3::UdpSocketFactory", sinkAddr);
      onOff.SetAttribute ("DataRate", StringValue ("54Mbps"));
      onOff.SetAttribute ("PacketSize", UintegerValue (1472));
      onOff.SetAttribute ("StartTime", TimeValue (Seconds (1.0)));
      onOff.SetAttribute ("StopTime", TimeValue (Seconds (simulationTime)));

      ApplicationContainer senderApp = onOff.Install (pairs[i].ap);
      senderApp.Start (Seconds (1.0));
      senderApp.Stop (Seconds (simulationTime));
      pairs[i].senderApp = senderApp.Get (0);
    }

  // Statistics/metrics objects
  for (uint32_t i = 0; i < nApStaPairs; ++i)
    {
      Ptr<NodeStatistics> stat = CreateObject<NodeStatistics> (pairs[i].sta, pairs[i].staDev);
      stat->SetPacketSinkApp (pairs[i].sinkApp);
      pairs[i].stat = stat;
    }

  // Schedule stats logging/plot output
  std::string thptFile = "thpt.txt";
  std::string txpowFile = "txpow.txt";
  std::string stateFile = "states.txt";
  std::ofstream thptStream (thptFile);
  std::ofstream txpowStream (txpowFile);
  std::ofstream stateStream (stateFile);

  Simulator::Schedule (Seconds (1), &PeriodicStats, std::ref (pairs), 1.0,
                       std::ref (thptStream), std::ref (txpowStream), std::ref (stateStream));

  // Gnuplot setup
  Gnuplot thptPlot ("sta-throughput.plt");
  Gnuplot txplot ("txpower.plt");
  Gnuplot statePlot ("states.plt");
  SetupGnuplots (thptPlot, txplot, statePlot, thptFile, txpowFile, stateFile);

  // FlowMonitor
  Ptr<FlowMonitor> flowmon;
  FlowMonitorHelper flowHelper;
  flowmon = flowHelper.InstallAll();

  Simulator::Stop (Seconds (simulationTime));
  Simulator::Run ();

  thptStream.close ();
  txpowStream.close ();
  stateStream.close ();

  // Final Gnuplot: plot files
  thptPlot.GenerateOutput (std::cout);
  txplot.GenerateOutput (std::cout);
  statePlot.GenerateOutput (std::cout);

  // Flow monitor stats output
  flowmon->CheckForLostPackets ();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowHelper.GetClassifier ());

  std::map<int,int> apMap; // protocol port -> pair idx
  for (uint32_t i = 0; i < nApStaPairs; ++i)
    apMap[basePort+i] = i;

  std::cout << "\nFlow Statistics Per AP-STA Pair:" << std::endl;
  FlowMonitor::FlowStatsContainer stats = flowmon->GetFlowStats ();
  for (auto iter = stats.begin (); iter != stats.end (); ++iter)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (iter->first);
      int pairIdx = apMap.count (t.destinationPort) ? apMap[t.destinationPort] : -1;
      std::cout << "Pair " << pairIdx << " (AP " << t.sourceAddress << " -> STA " << t.destinationAddress << "):" << std::endl;
      std::cout << "\tThroughput: " << iter->second.rxBytes * 8.0 / (simulationTime * 1e6) << " Mbps" << std::endl;
      std::cout << "\tMean delay: "
                << (iter->second.delaySum.GetSeconds () / iter->second.rxPackets) << " s" << std::endl;
      std::cout << "\tMean jitter: "
                << (iter->second.jitterSum.GetSeconds () / iter->second.rxPackets) << " s" << std::endl;
      std::cout << "\tLost Packets: " << iter->second.lostPackets << std::endl;
    }

  Simulator::Destroy ();
  return 0;
}