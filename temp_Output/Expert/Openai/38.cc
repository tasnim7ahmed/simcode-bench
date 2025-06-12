#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/gnuplot.h"
#include <vector>

using namespace ns3;

class NodeStatistics : public Object
{
public:
  NodeStatistics (Ptr<Node> node, Ptr<NetDevice> dev)
    : m_node (node),
      m_dev (DynamicCast<WifiNetDevice> (dev)),
      m_lastStateChange (Seconds (0.0)),
      m_lastState (WifiPhy::IDLE),
      m_stateTimes{0.0, 0.0, 0.0, 0.0},
      m_rxBytes (0),
      m_txPowerSum (0.0),
      m_txCount (0)
  {
    m_phy = m_dev->GetPhy ();
    m_phy->TraceConnectWithoutContext ("State", MakeCallback (&NodeStatistics::PhyStateTrace, this));
    m_phy->TraceConnectWithoutContext ("PhyTxBegin", MakeCallback (&NodeStatistics::TxBeginTrace, this));
  }

  void RxPacket (Ptr<const Packet> pkt, const Address &from)
  {
    m_rxBytes += pkt->GetSize ();
  }

  void PrintAndResetStats (double interval, std::ostream &os, double time)
  {
    double avgTxPower = (m_txCount > 0) ? m_txPowerSum / m_txCount : 0.0;
    double throughput = (m_rxBytes * 8.0) / (interval * 1e6); // Mbps

    os << time << "s\t" << throughput << "\t" << avgTxPower
       << "\t" << m_stateTimes[WifiPhy::CCA_BUSY]
       << "\t" << m_stateTimes[WifiPhy::IDLE]
       << "\t" << m_stateTimes[WifiPhy::TX]
       << "\t" << m_stateTimes[WifiPhy::RX] << std::endl;

    m_rxBytes = 0;
    for (int i = 0; i < 4; ++i)
      m_stateTimes[i] = 0.0;
    m_txPowerSum = 0.0;
    m_txCount = 0;
  }

  void ScheduleNextStat (double interval, std::ostream &os, double nextPrint)
  {
    Simulator::Schedule (Seconds (interval), &NodeStatistics::PrintAndResetStats, this, interval, std::ref(os), nextPrint);
    Simulator::Schedule (Seconds (interval), &NodeStatistics::ScheduleNextStat, this, interval, std::ref(os), nextPrint + interval);
  }

private:
  void PhyStateTrace (Time start, Time duration, WifiPhy::State state)
  {
    Time now = Simulator::Now ();
    if (m_lastState < 4)
      m_stateTimes[m_lastState] += (now - m_lastStateChange).GetSeconds ();
    m_lastStateChange = now;
    m_lastState = state;
  }

  void TxBeginTrace (Ptr<const Packet> p)
  {
    double txPower = m_phy->GetTxPowerStart ();
    m_txPowerSum += txPower;
    m_txCount++;
  }

  Ptr<Node> m_node;
  Ptr<WifiNetDevice> m_dev;
  Ptr<WifiPhy> m_phy;
  Time m_lastStateChange;
  WifiPhy::State m_lastState;
  double m_stateTimes[4]; // Indexed by CCA_BUSY=0, IDLE=1, TX=2, RX=3
  uint64_t m_rxBytes;

  double m_txPowerSum;
  uint32_t m_txCount;
};

// Helper for Gnuplot dataset collection
struct StatPoint
{
  double time;
  double throughput;
  double avgTxPower;
  double ccaBusy, idle, tx, rx;
};
std::vector<std::vector<StatPoint>> g_stats(2);

void PeriodicStats (Ptr<NodeStatistics> stats, double interval, int idx)
{
  static double lastTime[2] = {0,0};
  double t = Simulator::Now ().GetSeconds ();
  std::ostringstream dummy;
  std::ostringstream ss;
  std::streambuf *oldbuf = ss.rdbuf();
  stats->PrintAndResetStats (interval, ss, t);

  std::string line;
  std::getline (ss, line);
  double throughput, avgTx, cca, idle, tx, rx, dmy;
  std::istringstream ls(line);
  ls >> dmy >> throughput >> avgTx >> cca >> idle >> tx >> rx;
  g_stats[idx].push_back({t, throughput, avgTx, cca, idle, tx, rx});
  Simulator::Schedule (Seconds (interval), &PeriodicStats, stats, interval, idx);
}

int main (int argc, char *argv[])
{
  std::string wifiManager = "ns3::ParfWifiManager";
  uint32_t rtsThreshold = 2347;
  double txPowerStart = 16.0206; // Default 16dBm (40mW)
  double txPowerEnd = 16.0206;
  uint32_t simTime = 100;
  // Default positions
  double ap1x = 0.0, ap1y = 0.0;
  double sta1x = 5.0, sta1y = 0.0;
  double ap2x = 50.0, ap2y = 0.0;
  double sta2x = 45.0, sta2y = 0.0;

  CommandLine cmd;
  cmd.AddValue ("wifiManager", "Wifi Manager to use (e.g., ns3::ParfWifiManager, ns3::AarfWifiManager, etc.)", wifiManager);
  cmd.AddValue ("rtsThreshold", "RTS/CTS threshold", rtsThreshold);
  cmd.AddValue ("txPowerStart", "Wifi TxPowerStart (dBm)", txPowerStart);
  cmd.AddValue ("txPowerEnd", "Wifi TxPowerEnd (dBm)", txPowerEnd);
  cmd.AddValue ("simTime", "Simulation time (s)", simTime);

  cmd.AddValue ("ap1x", "AP1 X position", ap1x);
  cmd.AddValue ("ap1y", "AP1 Y position", ap1y);
  cmd.AddValue ("sta1x", "STA1 X position", sta1x);
  cmd.AddValue ("sta1y", "STA1 Y position", sta1y);
  cmd.AddValue ("ap2x", "AP2 X position", ap2x);
  cmd.AddValue ("ap2y", "AP2 Y position", ap2y);
  cmd.AddValue ("sta2x", "STA2 X position", sta2x);
  cmd.AddValue ("sta2y", "STA2 Y position", sta2y);

  cmd.Parse (argc, argv);

  NodeContainer apNodes, staNodes;
  apNodes.Create (2);
  staNodes.Create (2);

  NodeContainer allNodes;
  allNodes.Add (apNodes);
  allNodes.Add (staNodes);

  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  positionAlloc->Add (Vector (ap1x, ap1y, 0.0));
  positionAlloc->Add (Vector (ap2x, ap2y, 0.0));
  positionAlloc->Add (Vector (sta1x, sta1y, 0.0));
  positionAlloc->Add (Vector (sta2x, sta2y, 0.0));
  mobility.SetPositionAllocator (positionAlloc);
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (allNodes);

  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  wifiPhy.SetChannel (wifiChannel.Create ());
  wifiPhy.Set ("TxPowerStart", DoubleValue (txPowerStart));
  wifiPhy.Set ("TxPowerEnd", DoubleValue (txPowerEnd));
  wifiPhy.Set ("RxNoiseFigure", DoubleValue (7.0));

  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211a);
  wifi.SetRemoteStationManager (wifiManager, "RtsCtsThreshold", UintegerValue (rtsThreshold));

  WifiMacHelper wifiMac;

  NetDeviceContainer apDevices, staDevices;

  Ssid ssids[2] = { Ssid ("ap1"), Ssid ("ap2") };

  for (int i = 0; i < 2; ++i)
    {
      NetDeviceContainer apDev, staDev;
      // AP
      wifiMac.SetType ("ns3::ApWifiMac", "Ssid", SsidValue (ssids[i]));
      apDev = wifi.Install (wifiPhy, wifiMac, apNodes.Get (i));
      // STA
      wifiMac.SetType ("ns3::StaWifiMac", "Ssid", SsidValue (ssids[i]));
      staDev = wifi.Install (wifiPhy, wifiMac, staNodes.Get (i));
      apDevices.Add (apDev);
      staDevices.Add (staDev);
    }

  InternetStackHelper stack;
  stack.Install (allNodes);

  Ipv4AddressHelper address;
  Ipv4InterfaceContainer apIfs, staIfs;

  for (uint32_t i = 0; i < 2; ++i)
    {
      address.SetBase ("10.1." + std::to_string (i+1) + ".0", "255.255.255.0");
      Ipv4InterfaceContainer apIf = address.Assign (apDevices.Get (i));
      Ipv4InterfaceContainer staIf = address.Assign (staDevices.Get (i));
      apIfs.Add (apIf);
      staIfs.Add (staIf);
    }

  uint16_t port1 = 9001, port2 = 9002;
  ApplicationContainer sinkApps;

  // Packet Sinks on STAs
  for (uint32_t i = 0; i < 2; ++i)
    {
      PacketSinkHelper sinkHelper ("ns3::UdpSocketFactory",
                                   InetSocketAddress (Ipv4Address::GetAny (), i==0?port1:port2));
      sinkApps.Add (sinkHelper.Install (staNodes.Get (i)));
    }
  sinkApps.Start (Seconds (0.0));
  sinkApps.Stop (Seconds (simTime));

  // OnOff (CBR) UDP streams from APs to STAs
  ApplicationContainer onOffApps;
  for (uint32_t i = 0; i < 2; ++i)
    {
      InetSocketAddress dstAddr (staIfs.GetAddress (i), i==0?port1:port2);
      OnOffHelper onoff ("ns3::UdpSocketFactory", dstAddr);
      onoff.SetAttribute ("DataRate", StringValue ("54Mbps"));
      onoff.SetAttribute ("PacketSize", UintegerValue (1460));
      onoff.SetAttribute ("StartTime", TimeValue (Seconds (1.0)));
      onoff.SetAttribute ("StopTime", TimeValue (Seconds (simTime-1)));
      onoff.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
      onoff.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
      onOffApps.Add (onoff.Install (apNodes.Get (i)));
    }

  std::vector<Ptr<NodeStatistics>> statsVec;
  std::vector<std::ostringstream> statsOuts(2);
  std::vector<Ptr<PacketSink>> sinkPtrs;

  for (uint32_t i = 0; i < 2; ++i)
    {
      Ptr<PacketSink> sink = DynamicCast<PacketSink> (sinkApps.Get (i));
      sinkPtrs.push_back (sink);

      Ptr<NodeStatistics> nodeStat = CreateObject<NodeStatistics> (staNodes.Get (i), staDevices.Get (i));
      statsVec.push_back (nodeStat);
      sink->TraceConnectWithoutContext ("Rx", MakeBoundCallback (&NodeStatistics::RxPacket, nodeStat));
    }

  for (uint32_t i = 0; i < 2; ++i)
    {
      Simulator::Schedule (Seconds (1), &PeriodicStats, statsVec[i], 1.0, i);
    }

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  Simulator::Stop (Seconds (simTime));
  Simulator::Run ();

  // Print summary stats
  std::cout << "Time(s)\tThroughput(Mbps)\tAvgTxPwr(dBm)\tCCA_BUSY(s)\tIDLE(s)\tTX(s)\tRX(s)" << std::endl;
  for (uint32_t i = 0; i < 2; ++i)
    {
      for (const auto& p : g_stats[i])
        {
          std::cout << "STA" << (i+1) << " " << p.time << "\t" << p.throughput << "\t"
                    << p.avgTxPower << "\t" << p.ccaBusy << "\t" << p.idle << "\t"
                    << p.tx << "\t" << p.rx << std::endl;
        }
    }

  // Gnuplot
  Gnuplot throughputPlot ("throughput.png");
  throughputPlot.SetTitle ("STA Throughput (Mbps)");
  throughputPlot.SetLegend ("Time (s)", "Throughput (Mbps)");
  Gnuplot2dDataset th1 ("STA1"), th2 ("STA2");
  for (const auto& p : g_stats[0])
    th1.Add (p.time, p.throughput);
  for (const auto& p : g_stats[1])
    th2.Add (p.time, p.throughput);
  throughputPlot.AddDataset (th1);
  throughputPlot.AddDataset (th2);
  std::ofstream file1 ("throughput.plt");
  throughputPlot.GenerateOutput (file1);
  file1.close ();

  Gnuplot txpPlot ("avg_txpower.png");
  txpPlot.SetTitle ("Average TX Power (dBm)");
  txpPlot.SetLegend ("Time (s)", "Avg TX Power (dBm)");
  Gnuplot2dDataset tp1 ("STA1"), tp2 ("STA2");
  for (const auto& p : g_stats[0])
    tp1.Add (p.time, p.avgTxPower);
  for (const auto& p : g_stats[1])
    tp2.Add (p.time, p.avgTxPower);
  txpPlot.AddDataset (tp1);
  txpPlot.AddDataset (tp2);
  std::ofstream file2 ("txpower.plt");
  txpPlot.GenerateOutput (file2);
  file2.close ();

  Gnuplot statePlot ("phystates.png");
  statePlot.SetTitle ("WiFi PHY States (per second)");
  statePlot.SetLegend ("Time (s)", "State Times (s)");
  Gnuplot2dDataset cca1 ("STA1 CCA_BUSY"), idle1("STA1 IDLE"), tx1("STA1 TX"), rx1("STA1 RX");
  Gnuplot2dDataset cca2 ("STA2 CCA_BUSY"), idle2("STA2 IDLE"), tx2("STA2 TX"), rx2("STA2 RX");
  for (const auto& p : g_stats[0])
    {
      cca1.Add (p.time, p.ccaBusy);
      idle1.Add (p.time, p.idle);
      tx1.Add (p.time, p.tx);
      rx1.Add (p.time, p.rx);
    }
  for (const auto& p : g_stats[1])
    {
      cca2.Add (p.time, p.ccaBusy);
      idle2.Add (p.time, p.idle);
      tx2.Add (p.time, p.tx);
      rx2.Add (p.time, p.rx);
    }
  statePlot.AddDataset (cca1); statePlot.AddDataset (idle1); statePlot.AddDataset (tx1); statePlot.AddDataset (rx1);
  statePlot.AddDataset (cca2); statePlot.AddDataset (idle2); statePlot.AddDataset (tx2); statePlot.AddDataset (rx2);
  std::ofstream file3 ("phystates.plt");
  statePlot.GenerateOutput (file3);
  file3.close ();

  // FlowMonitor stats
  monitor->CheckForLostPackets ();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats ();

  std::cout << "\nFlowMonitor results:\nFlowId\tSrc\tDst\tThroughput(Mbps)\tDelay(ms)\tJitter(ms)\n";
  for (const auto &flow : stats)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (flow.first);
      double throughput = flow.second.rxBytes * 8.0 / (simTime * 1000000.0);
      double delayMs = (flow.second.delaySum.GetSeconds ()/(double)flow.second.rxPackets)*1000;
      double jitterMs = (flow.second.jitterSum.GetSeconds ()/(double)flow.second.rxPackets)*1000;
      std::cout << flow.first << "\t" << t.sourceAddress << "\t" << t.destinationAddress
                << "\t" << throughput << "\t" << delayMs << "\t" << jitterMs << std::endl;
    }

  Simulator::Destroy ();
  return 0;
}