#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/ssid.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/gnuplot.h"
#include <vector>
#include <map>
#include <string>
#include <sstream>
#include <iomanip>

using namespace ns3;

// NodeStatistics Class
class NodeStatistics : public Object
{
public:
  NodeStatistics(Ptr<Node> node, Ptr<NetDevice> dev)
    : m_node (node), m_dev (dev),
      m_ccaBusyTime (Seconds (0)), m_idleTime (Seconds (0)),
      m_txTime (Seconds (0)), m_rxTime (Seconds (0)),
      m_lastStateTime (Seconds (0)), m_bytesRx (0),
      m_lastTxPowerSum (0.0), m_lastTxCount (0),
      m_currentState (WifiPhy::IDLE), m_lastSampleTime (Seconds (0)) {}

  virtual void DoDispose() override
  {
    m_rxTrace.Disconnect();
    m_phyStateTrace.Disconnect();
    m_txPowerTrace.Disconnect();
    Object::DoDispose();
  }

  void ConnectTraces(Ptr<WifiPhy> phy, Ptr<Application> sinkApp)
  {
    m_phy = phy;
    m_rxTrace = phy->TraceConnectWithoutContext (
      "MonitorSnifferRx", MakeCallback (&NodeStatistics::SnifferRx, this));
    m_phyStateTrace = phy->TraceConnectWithoutContext (
      "State", MakeCallback (&NodeStatistics::PhyStateChangeTrace, this));
    m_txPowerTrace = phy->TraceConnectWithoutContext (
      "TxPower", MakeCallback (&NodeStatistics::TxPowerTrace, this));
    m_pktSinkTrace = sinkApp->TraceConnectWithoutContext (
      "Rx", MakeCallback (&NodeStatistics::PktSinkRx, this));
    m_lastStateTime = Seconds (0);
    m_lastSampleTime = Simulator::Now();
  }

  void SnifferRx (Ptr<const Packet> packet)
  {
    // Not used, possibly future extensions
  }

  void PktSinkRx (Ptr<const Packet> packet, const Address &from)
  {
    m_bytesRx += packet->GetSize();
  }

  void PhyStateChangeTrace (Time start, Time duration, WifiPhy::State state)
  {
    Time now = Simulator::Now();
    Time elapsed = now - m_lastStateTime;
    switch (m_currentState)
      {
      case WifiPhy::CCA_BUSY: m_ccaBusyTime += elapsed; break;
      case WifiPhy::IDLE:     m_idleTime += elapsed; break;
      case WifiPhy::TX:       m_txTime += elapsed; break;
      case WifiPhy::RX:       m_rxTime += elapsed; break;
      default: break;
      }
    m_currentState = state;
    m_lastStateTime = now;
  }

  void TxPowerTrace (double txPower)
  {
    m_lastTxPowerSum += txPower;
    m_lastTxCount++;
  }

  void ResetCounters()
  {
    m_ccaBusyTime = Seconds (0);
    m_idleTime = Seconds (0);
    m_txTime = Seconds (0);
    m_rxTime = Seconds (0);
    m_bytesRx = 0;
    m_lastTxPowerSum = 0.0;
    m_lastTxCount = 0;
    m_lastSampleTime = Simulator::Now();
    m_lastStateTime = m_lastSampleTime;
  }

  void Sample(Time now)
  {
    // Account time in current state since last event
    Time elapsed = now - m_lastStateTime;
    switch (m_currentState)
      {
      case WifiPhy::CCA_BUSY: m_ccaBusyTime += elapsed; break;
      case WifiPhy::IDLE:     m_idleTime += elapsed; break;
      case WifiPhy::TX:       m_txTime += elapsed; break;
      case WifiPhy::RX:       m_rxTime += elapsed; break;
      default: break;
      }
    m_lastStateTime = now;
  }

  double GetThroughputMbps(Time interval) const
  {
    return (m_bytesRx * 8.0) / interval.GetSeconds() / (1e6);
  }

  double GetAvgTxPower() const
  {
    if (m_lastTxCount == 0)
      return 0.0;
    return m_lastTxPowerSum / m_lastTxCount;
  }

  double GetCCA_BusyFraction(Time interval) const
  {
    return m_ccaBusyTime.GetSeconds() / interval.GetSeconds();
  }

  double GetIdleFraction(Time interval) const
  {
    return m_idleTime.GetSeconds() / interval.GetSeconds();
  }

  double GetTxFraction(Time interval) const
  {
    return m_txTime.GetSeconds() / interval.GetSeconds();
  }

  double GetRxFraction(Time interval) const
  {
    return m_rxTime.GetSeconds() / interval.GetSeconds();
  }

  uint64_t GetTotalRx() const { return m_bytesRx; }
  std::string GetNodeName() const
  {
    Ptr<Node> nd = m_node;
    if (nd->GetDevice(0)->GetInstanceTypeId() == WifiNetDevice::GetTypeId())
      {
        std::ostringstream oss;
        oss << (nd->GetId ());
        return oss.str();
      }
    return "";
  }

private:
  Ptr<Node> m_node;
  Ptr<NetDevice> m_dev;
  Ptr<WifiPhy> m_phy;
  TracedCallback<Ptr<const Packet>, const Address &> m_pktSinkTrace;
  TracedCallback<Ptr<const Packet> > m_rxTrace;
  TracedCallback<Time, Time, WifiPhy::State> m_phyStateTrace;
  TracedCallback<double> m_txPowerTrace;

  Time m_ccaBusyTime, m_idleTime, m_txTime, m_rxTime;
  Time m_lastStateTime;
  uint64_t m_bytesRx;
  double m_lastTxPowerSum;
  uint32_t m_lastTxCount;
  WifiPhy::State m_currentState;
  Time m_lastSampleTime;
};

// Helper for Gnuplot curves
class TimeSeriesCollector
{
public:
  struct SeriesItem
  {
    double t;
    double v;
    SeriesItem (double _t, double _v): t(_t), v(_v) {}
  };
  std::vector<SeriesItem> throughput[2];
  std::vector<SeriesItem> txPower[2];
  std::vector<SeriesItem> idleFrac[2];
  std::vector<SeriesItem> txFrac[2];
  std::vector<SeriesItem> rxFrac[2];
  std::vector<SeriesItem> ccaBusyFrac[2];

  void AddSample (double t, unsigned idx,
                  double thr, double txp, double idlef, double txf, double rxf, double ccaf)
  {
    throughput[idx].emplace_back (t, thr);
    txPower[idx].emplace_back (t, txp);
    idleFrac[idx].emplace_back (t, idlef);
    txFrac[idx].emplace_back (t, txf);
    rxFrac[idx].emplace_back (t, rxf);
    ccaBusyFrac[idx].emplace_back (t, ccaf);
  }
  void WritePlots()
  {
    for (unsigned i = 0; i < 2; ++i)
      {
        std::ostringstream filen, title;
        filen << "throughput_sta" << i << ".plt";
        title << "Throughput STA" << i;
        Gnuplot plot (filen.str());
        Gnuplot2dDataset dataset ("Throughput(Mbps)");
        for (const auto &item : throughput[i])
          dataset.Add (item.t, item.v);
        plot.SetTitle (title.str());
        plot.SetTerminal ("png");
        plot.SetLegend ("Time(s)", "Throughput (Mbps)");
        plot.AddDataset (dataset);
        plot.GenerateOutput (std::cout);

        std::ostringstream fname_tp;
        fname_tp << "avg_txpower_sta" << i << ".plt";
        Gnuplot tpplot (fname_tp.str());
        Gnuplot2dDataset tpds ("Avg Tx Power (dBm)");
        for (const auto &item : txPower[i])
          tpds.Add (item.t, item.v);
        tpplot.SetTitle ("Tx Power STA" + std::to_string(i));
        tpplot.SetTerminal ("png");
        tpplot.SetLegend ("Time(s)", "Avg Tx Power (dBm)");
        tpplot.AddDataset(tpds);
        tpplot.GenerateOutput (std::cout);

        std::ostringstream fname_idle;
        fname_idle << "wifi_state_fractions_sta" << i << ".plt";
        Gnuplot stateplot (fname_idle.str());
        Gnuplot2dDataset id("Idle"), tx("Tx"), rx("Rx"), cca("CCA_BUSY");
        for (const auto &item : idleFrac[i])
          id.Add (item.t, item.v);
        for (const auto &item : txFrac[i])
          tx.Add (item.t, item.v);
        for (const auto &item : rxFrac[i])
          rx.Add (item.t, item.v);
        for (const auto &item : ccaBusyFrac[i])
          cca.Add (item.t, item.v);
        stateplot.SetTitle ("State Fractions STA" + std::to_string(i));
        stateplot.SetTerminal ("png");
        stateplot.SetLegend ("Time(s)", "Fraction");
        stateplot.AddDataset(id);
        stateplot.AddDataset(tx);
        stateplot.AddDataset(rx);
        stateplot.AddDataset(cca);
        stateplot.GenerateOutput (std::cout);
      }
  }
};

// Sampling & reporting functor
void
SampleStatistics (std::vector<Ptr<NodeStatistics>> nodeStats, Time interval, Time stopTime, TimeSeriesCollector *tsc)
{
  static uint32_t sampleIdx = 0;
  std::cout << "==== Statistics at time " << std::fixed << std::setprecision (2)
            << Simulator::Now ().GetSeconds () << "s ====" << std::endl;
  for (unsigned i = 0; i < nodeStats.size (); ++i)
    {
      Ptr<NodeStatistics> stats = nodeStats[i];
      stats->Sample (Simulator::Now());
      double thr = stats->GetThroughputMbps(interval);
      double avgtx = stats->GetAvgTxPower();
      double idlef = stats->GetIdleFraction(interval);
      double txf = stats->GetTxFraction(interval);
      double rxf = stats->GetRxFraction(interval);
      double ccaf = stats->GetCCA_BusyFraction(interval);

      tsc->AddSample (Simulator::Now().GetSeconds(), i, thr, avgtx, idlef, txf, rxf, ccaf);

      std::cout << "STA " << i
                << " Throughput=" << thr << " Mbps"
                << " AvgTxPower=" << avgtx << " dBm"
                << " Idle=" << idlef * 100 << "%"
                << " TX=" << txf * 100 << "%"
                << " RX=" << rxf * 100 << "%"
                << " CCA_BUSY=" << ccaf * 100 << "%" << std::endl;

      stats->ResetCounters();
    }
  Time next = Simulator::Now () + interval;
  if (next <= stopTime)
    Simulator::Schedule (interval, &SampleStatistics, nodeStats, interval, stopTime, tsc);
}

// Helper to set positions from command line
void
ParsePositions (std::string posString, std::vector<Vector> &positions, unsigned n)
{
  std::istringstream iss (posString);
  double x, y, z;
  char c;
  positions.clear ();
  while (iss >> x >> c && c == ',' && iss >> y >> c && c == ',' && iss >> z)
    {
      positions.emplace_back (x, y, z);
      if (!iss.eof())
        iss >> c;
    }
  // If not enough, pad with zeros
  while (positions.size () < n)
    positions.emplace_back (0,0,0);
}

// main
int main(int argc, char *argv[])
{
  // Parameters
  std::string apPositions = "0,0,0 10,0,0";
  std::string staPositions = "0,10,0 10,10,0";
  std::string wifiManager = "ns3::ParfWifiManager";
  std::string rtsThreshold = "2200";
  double txPowerStart = 16.0206;
  double txPowerEnd = 16.0206;
  uint32_t simTime = 100;
  double rateMbps = 54.0;

  CommandLine cmd;
  cmd.AddValue ("apPositions", "AP positions as 'x1,y1,z1 x2,y2,z2'", apPositions);
  cmd.AddValue ("staPositions", "STA positions as 'x1,y1,z1 x2,y2,z2'", staPositions);
  cmd.AddValue ("wifiManager", "WiFi manager (e.g., ns3::ParfWifiManager)",
                wifiManager);
  cmd.AddValue ("rtsThreshold", "RTS/CTS threshold", rtsThreshold);
  cmd.AddValue ("txPowerStart", "Transmission power start (dBm)", txPowerStart);
  cmd.AddValue ("txPowerEnd", "Transmission power end (dBm)", txPowerEnd);
  cmd.AddValue ("simTime", "Simulation time (seconds)", simTime);
  cmd.Parse (argc, argv);

  std::vector<Vector> apPos, staPos;
  ParsePositions (apPositions, apPos, 2);
  ParsePositions (staPositions, staPos, 2);

  NodeContainer aps, stas;
  aps.Create (2);
  stas.Create (2);
  NodeContainer allNodes;
  allNodes.Add (aps);
  allNodes.Add (stas);

  // Mobility
  MobilityHelper mobility;
  Ptr<ListPositionAllocator> posAlloc = CreateObject<ListPositionAllocator> ();
  posAlloc->Add (apPos[0]);
  posAlloc->Add (apPos[1]);
  posAlloc->Add (staPos[0]);
  posAlloc->Add (staPos[1]);
  mobility.SetPositionAllocator (posAlloc);
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (allNodes);

  // WiFi Config
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());
  phy.Set ("TxPowerStart", DoubleValue (txPowerStart));
  phy.Set ("TxPowerEnd", DoubleValue (txPowerEnd));
  phy.Set ("RxGain", DoubleValue (0));
  phy.Set ("TxGain", DoubleValue (0));

  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211a);
  wifi.SetRemoteStationManager (wifiManager);

  WifiMacHelper mac;

  NetDeviceContainer apDevs[2], staDevs[2];

  // AP<->STA pairs (2 nets)
  for (int i = 0; i < 2; ++i)
    {
      std::ostringstream ssidStr; ssidStr << "ssid-" << i;
      Ssid ssid = Ssid (ssidStr.str ());
      // AP
      mac.SetType ("ns3::ApWifiMac",
                   "Ssid", SsidValue (ssid),
                   "RtsCtsThreshold", StringValue(rtsThreshold));
      apDevs[i] = wifi.Install (phy, mac, NodeContainer (aps.Get(i)));
      // STA
      mac.SetType ("ns3::StaWifiMac",
                   "Ssid", SsidValue (ssid),
                   "ActiveProbing", BooleanValue (false),
                   "RtsCtsThreshold", StringValue(rtsThreshold));
      staDevs[i] = wifi.Install (phy, mac, NodeContainer (stas.Get(i)));
    }

  // Internet stack
  InternetStackHelper stack;
  stack.Install (allNodes);

  // Address assignments (two /30 nets)
  Ipv4AddressHelper ipv4;
  std::vector<Ipv4InterfaceContainer> apIfs, staIfs;
  for (int i = 0; i < 2; ++i)
    {
      std::ostringstream net;
      net << "10.1." << i << ".0";
      ipv4.SetBase (Ipv4Address (net.str ().c_str ()), "255.255.255.252");
      Ipv4InterfaceContainer tempIfs = ipv4.Assign (NetDeviceContainer (apDevs[i], staDevs[i]));
      apIfs.push_back (Ipv4InterfaceContainer (tempIfs.Get(0)));
      staIfs.push_back (Ipv4InterfaceContainer (tempIfs.Get(1)));
    }

  // Routing
  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  // Applications: PacketSink on STAs, UDP OnOff on APs
  ApplicationContainer sinkApps, onoffApps;
  uint16_t port[2] = {5000, 5001};
  Address staAddr[2] = {
    InetSocketAddress (staIfs[0].GetAddress (0), port[0]),
    InetSocketAddress (staIfs[1].GetAddress (0), port[1])
  };
  for (int i = 0; i < 2; ++i)
    {
      PacketSinkHelper sinkHelper ("ns3::UdpSocketFactory", staAddr[i]);
      ApplicationContainer staApp = sinkHelper.Install (stas.Get (i));
      staApp.Start (Seconds (0));
      staApp.Stop (Seconds (simTime));
      sinkApps.Add (staApp);

      OnOffHelper onoff ("ns3::UdpSocketFactory", staAddr[i]);
      std::ostringstream rate;
      rate << std::fixed << std::setprecision (1) << rateMbps << "Mbps";
      onoff.SetAttribute ("DataRate", StringValue (rate.str ()));
      onoff.SetAttribute ("PacketSize", UintegerValue (1472));
      onoff.SetAttribute ("StartTime", TimeValue (Seconds (1.0)));
      onoff.SetAttribute ("StopTime", TimeValue (Seconds (simTime)));
      onoff.SetAttribute ("OnTime",  StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
      onoff.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
      ApplicationContainer apApp = onoff.Install (aps.Get (i));
      onoffApps.Add (apApp);
    }

  // Statistics objects for each STA
  std::vector<Ptr<NodeStatistics>> nodeStats;
  TypeId phyType = WifiPhy::GetTypeId ();
  for (uint32_t i = 0; i < 2; ++i)
    {
      // Each STA: one device, one phy
      Ptr<WifiNetDevice> staWifiDev = DynamicCast<WifiNetDevice> (staDevs[i].Get (0));
      Ptr<WifiPhy> staPhy = staWifiDev->GetPhy ();
      Ptr<Application> sinkApp = sinkApps.Get (i);
      Ptr<NodeStatistics> ns = CreateObject<NodeStatistics> (stas.Get (i), staDevs[i].Get (0));
      ns->ConnectTraces (staPhy, sinkApp);
      nodeStats.push_back (ns);
    }

  // Time series collectors for plots
  TimeSeriesCollector tsCollector;
  Time sampleInterval = Seconds (1.0);

  // Schedule per-second reporting
  Simulator::Schedule (Seconds (1.0), &SampleStatistics,
                       nodeStats, sampleInterval, Seconds (simTime), &tsCollector);

  // FlowMonitor
  FlowMonitorHelper flowmonHelper;
  Ptr<FlowMonitor> flowmon = flowmonHelper.InstallAll();

  Simulator::Stop (Seconds (simTime));
  Simulator::Run ();

  // Print FlowMonitor statistics
  flowmon->CheckForLostPackets ();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (
      flowmonHelper.GetClassifier ());

  std::map<FlowId, FlowMonitor::FlowStats> stats = flowmon->GetFlowStats ();
  std::cout << "\n=== Flow Monitor Stats ===" << std::endl;

  for (const auto &sf : stats)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (sf.first);
      std::cout << "FlowID: " << sf.first
                << " (" << t.sourceAddress << " --> "
                << t.destinationAddress << ")\n";
      std::cout << "  Tx Packets: " << sf.second.txPackets << "\n";
      std::cout << "  Rx Packets: " << sf.second.rxPackets << "\n";
      std::cout << "  Lost Packets: " << sf.second.lostPackets << "\n";
      double duration = sf.second.timeLastRxPacket.GetSeconds () -
                        sf.second.timeFirstTxPacket.GetSeconds ();
      double throughput = (sf.second.rxBytes * 8.0) / (duration * 1e6);
      std::cout << "  Throughput: "
                << throughput << " Mbps\n";
      std::cout << "  Mean Delay: "
                << (sf.second.delaySum.GetSeconds () / sf.second.rxPackets) << " s\n";
      std::cout << "  Mean Jitter: "
                << (sf.second.jitterSum.GetSeconds () / (sf.second.rxPackets - 1)) << " s\n";
    }
  // Generate and save gnuplot data
  tsCollector.WritePlots();

  Simulator::Destroy ();
  return 0;
}