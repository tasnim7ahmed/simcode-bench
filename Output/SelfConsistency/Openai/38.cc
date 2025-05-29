/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/gnuplot.h"

#include <vector>
#include <string>
#include <map>
#include <iomanip>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("DualApStaStats");

// WiFi state mapping for easier statistics tracking
enum WifiPhyState
{
  WIFI_IDLE = 0,
  WIFI_CCA_BUSY,
  WIFI_TX,
  WIFI_RX,
  WIFI_STATES
};

static const char *wifiStateNames[WIFI_STATES] = {
  "IDLE", "CCA_BUSY", "TX", "RX"
};

class NodeStatistics : public Object
{
public:
  NodeStatistics ()
  {
    m_bytesReceived = 0;
    m_avgTxPower = 0.0;
    m_powerSamples = 0;
    for (uint32_t i = 0; i < WIFI_STATES; ++i)
      {
        m_stateTime[i] = Seconds (0.0);
      }
    m_lastState = WIFI_IDLE;
    m_lastStateChange = Seconds (0.0);
    m_lastRxTime = Seconds (0.0);
  }

  void NotifyRx (Ptr<const Packet> packet, const Address &src)
  {
    m_bytesReceived += packet->GetSize ();
    m_lastRxTime = Simulator::Now ();
  }

  void NotifyPhyStateChange (WifiPhyState newState)
  {
    Time now = Simulator::Now ();
    m_stateTime[m_lastState] += now - m_lastStateChange;
    m_lastState = newState;
    m_lastStateChange = now;
  }

  void NotifyTxPower (double txPowerDbm)
  {
    m_avgTxPower += txPowerDbm;
    ++m_powerSamples;
  }

  void End ()
  {
    // Account for last state segment until end of sim
    Time now = Simulator::Now ();
    m_stateTime[m_lastState] += now - m_lastStateChange;
  }

  uint64_t GetBytesReceived () const { return m_bytesReceived; }
  double GetAvgTxPower () const
  {
    return m_powerSamples == 0 ? 0.0 : m_avgTxPower / m_powerSamples;
  }
  double GetStateTime (WifiPhyState state) const
  {
    return m_stateTime[state].GetSeconds ();
  }
  double GetLastRxTimeSec () const
  {
    return m_lastRxTime.GetSeconds ();
  }

private:
  uint64_t m_bytesReceived;
  double   m_avgTxPower;
  uint64_t m_powerSamples;
  Time     m_stateTime[WIFI_STATES];
  WifiPhyState m_lastState;
  Time         m_lastStateChange;
  Time         m_lastRxTime;
};

class SimulationStats
{
public:
  SimulationStats (uint32_t nSta)
  {
    // Reserve space for node stats
    for (uint32_t i = 0; i < nSta; ++i)
      {
        m_nodeStats.push_back (CreateObject<NodeStatistics> ());
      }
    m_startTime = Simulator::Now ();
    m_throughputs.reserve (nSta);
    m_powers.reserve (nSta);
    for (uint32_t i = 0; i < nSta; ++i)
      {
        m_throughputs.push_back (std::vector<double> ());
        m_powers.push_back (std::vector<double> ());
        for (uint32_t j = 0; j < WIFI_STATES; ++j)
          m_stateTimes[i][j] = std::vector<double> ();
      }
  }

  Ptr<NodeStatistics> Get (uint32_t idx) { return m_nodeStats[idx]; }

  void Record (double elapsed)
  {
    // Print and gather stats for each STA node
    for (uint32_t i = 0; i < m_nodeStats.size (); ++i)
      {
        double bytes = m_nodeStats[i]->GetBytesReceived ();
        double throughput = bytes * 8.0 / (elapsed * 1e6); // Mbps
        double avgPower = m_nodeStats[i]->GetAvgTxPower ();
        m_throughputs[i].push_back (throughput);
        m_powers[i].push_back (avgPower);
        for (uint32_t st = 0; st < WIFI_STATES; ++st)
          {
            m_stateTimes[i][st].push_back (m_nodeStats[i]->GetStateTime ((WifiPhyState)st));
          }
        // Print header at t=0
        if (std::abs (elapsed - 1.0) < 1e-6)
          {
            std::cout << "Time(s)\tSTA\tRxBytes\tThroughput(Mbps)\tAvgTxPower(dBm)";
            for (uint32_t st = 0; st < WIFI_STATES; ++st)
              std::cout << "\tTime_" << wifiStateNames[st];
            std::cout << std::endl;
          }
        std::cout << std::fixed << std::setprecision (2);
        std::cout << elapsed << "\t"
                  << i << "\t"
                  << bytes << "\t"
                  << throughput << "\t"
                  << avgPower;
        for (uint32_t st = 0; st < WIFI_STATES; ++st)
          std::cout << "\t" << m_nodeStats[i]->GetStateTime ((WifiPhyState)st);
        std::cout << std::endl;
      }
  }

  void ScheduleNext (Time interval)
  {
    Simulator::Schedule (interval,
      &SimulationStats::PeriodicPrint, this, interval.GetSeconds ());
  }

  void PeriodicPrint (double intervalSec)
  {
    double elapsed = (Simulator::Now () - m_startTime).GetSeconds ();
    Record (elapsed);
    if (elapsed + intervalSec < m_duration)
      {
        Simulator::Schedule (Seconds (intervalSec),
            &SimulationStats::PeriodicPrint, this, intervalSec);
      }
    else
      {
        for (auto stat : m_nodeStats)
          stat->End ();
      }
  }

  void SetDuration (double duration) { m_duration = duration; }

  // Returns throughput history for a given STA
  const std::vector<double>& GetThroughputs (uint32_t idx) const { return m_throughputs[idx]; }
  // Average Tx power history for a given STA
  const std::vector<double>& GetPowers (uint32_t idx) const { return m_powers[idx]; }
  // State times for a given STA/state over time
  const std::vector<double>& GetStateTimes (uint32_t idx, uint32_t st) const { return m_stateTimes[idx][st]; }

private:
  std::vector< Ptr<NodeStatistics> > m_nodeStats;
  // For Gnuplotting
  std::vector< std::vector<double> > m_throughputs;
  std::vector< std::vector<double> > m_powers;
  std::vector< std::vector<double> > m_stateTimes[WIFI_STATES];

  Time m_startTime;
  double m_duration;
};

// Attach hooks to WifiPhy and NetDevice for statistics
void AttachPhyStateTrace (Ptr<WifiPhy> phy, Ptr<NodeStatistics> stats)
{
  phy->TraceConnectWithoutContext (
    "State",
    MakeCallback (
      [stats](Time start, Time duration, WifiPhyState state){
        // Only care about entering new state
        stats->NotifyPhyStateChange (state);
      }
    )
  );
  phy->TraceConnectWithoutContext (
    "TxPowerStart",
    MakeCallback (
      [stats](double txPowerDbm){
        stats->NotifyTxPower (txPowerDbm);
      }
    )
  );
}

void AttachRxTrace (Ptr<Application> app, Ptr<NodeStatistics> stats)
{
  Ptr<PacketSink> sink = DynamicCast<PacketSink> (app);
  if (sink)
    {
      sink->TraceConnectWithoutContext (
        "Rx",
        MakeCallback (
          [stats](Ptr<const Packet> packet, const Address &src){
            stats->NotifyRx (packet, src);
          }
        )
      );
    }
}

// Helper to create mobility model with configurable ListPositionAllocator
void InstallMobility (NodeContainer nodes, std::vector<Vector> positions)
{
  Ptr<ListPositionAllocator> posAlloc = CreateObject<ListPositionAllocator> ();
  for (auto &v : positions)
    posAlloc->Add (v);
  MobilityHelper mobility;
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.SetPositionAllocator (posAlloc);
  mobility.Install (nodes);
}

// Helper for creating Gnuplot files for each metric
void WriteGnuplot (std::string output, std::string title,
                   std::vector<double> x, std::vector< std::vector<double> > y,
                   std::vector<std::string> legends)
{
  Gnuplot plot (output);
  plot.SetTitle (title);
  plot.SetTerminal ("png");
  plot.SetLegend ("Time (s)", title.c_str ());
  for (uint32_t i = 0; i < y.size (); ++i)
    {
      Gnuplot2dDataset ds (legends[i]);
      for (uint32_t t = 0; t < x.size () && t < y[i].size (); ++t)
        ds.Add (x[t], y[i][t]);
      plot.AddDataset (ds);
    }
  std::ofstream out (output);
  plot.GenerateOutput (out);
  out.close ();
}

int main (int argc, char *argv[])
{
  uint32_t nAps = 2;
  uint32_t nStas = 2;
  double  simTime = 100.0;
  double  interval = 1.0;
  uint32_t rtsThreshold = 2347;
  double minTxPower = 16.0; // dBm
  double maxTxPower = 16.0; // dBm
  std::string wifiManager = "ns3::ParfWifiManager";
  // Default positions: AP at (0,0,0), STA at (5,0,0) and so on.
  std::vector<Vector> apPositions = { Vector (0, 0, 0), Vector (20, 0, 0) };
  std::vector<Vector> staPositions = { Vector (5, 0, 0), Vector (25, 0, 0) };

  // Parse CLI
  CommandLine cmd;
  cmd.AddValue ("simTime", "Simulation time (s)", simTime);
  cmd.AddValue ("wifiManager", "WiFi manager: ns3::ParfWifiManager (default), ns3::MinstrelWifiManager, etc.", wifiManager);
  cmd.AddValue ("rtsThreshold", "RTS/CTS threshold (bytes)", rtsThreshold);
  cmd.AddValue ("minTxPower", "Minimum Tx Power in dBm", minTxPower);
  cmd.AddValue ("maxTxPower", "Maximum Tx Power in dBm", maxTxPower);

  double ap0x = 0, ap0y = 0, ap0z = 0, ap1x = 20, ap1y = 0, ap1z = 0;
  double sta0x = 5, sta0y = 0, sta0z = 0, sta1x = 25, sta1y = 0, sta1z = 0;

  cmd.AddValue ("ap0x", "X position of AP0", ap0x);
  cmd.AddValue ("ap0y", "Y position of AP0", ap0y);
  cmd.AddValue ("ap0z", "Z position of AP0", ap0z);
  cmd.AddValue ("ap1x", "X position of AP1", ap1x);
  cmd.AddValue ("ap1y", "Y position of AP1", ap1y);
  cmd.AddValue ("ap1z", "Z position of AP1", ap1z);

  cmd.AddValue ("sta0x", "X position of STA0", sta0x);
  cmd.AddValue ("sta0y", "Y position of STA0", sta0y);
  cmd.AddValue ("sta0z", "Z position of STA0", sta0z);
  cmd.AddValue ("sta1x", "X position of STA1", sta1x);
  cmd.AddValue ("sta1y", "Y position of STA1", sta1y);
  cmd.AddValue ("sta1z", "Z position of STA1", sta1z);

  cmd.Parse (argc, argv);

  // Update positions if supplied
  apPositions[0] = Vector (ap0x, ap0y, ap0z);
  apPositions[1] = Vector (ap1x, ap1y, ap1z);
  staPositions[0] = Vector (sta0x, sta0y, sta0z);
  staPositions[1] = Vector (sta1x, sta1y, sta1z);

  //--- Create Nodes ---
  NodeContainer apNodes, staNodes;
  apNodes.Create (nAps);
  staNodes.Create (nStas);

  //--- Install Mobility ---
  InstallMobility (apNodes, apPositions);
  InstallMobility (staNodes, staPositions);

  //--- WiFi PHY/Channel ---
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  phy.SetChannel (channel.Create ());
  phy.Set ("TxPowerStart", DoubleValue (minTxPower));
  phy.Set ("TxPowerEnd", DoubleValue (maxTxPower));
  phy.Set ("CcaMode1Threshold", DoubleValue (-62.0)); // Default
  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211a);

  //--- WiFi MAC/SSID per AP ---
  std::vector<std::string> ssids = { "AP-0", "AP-1" };
  std::vector<Ssid> ssidObjs = { Ssid (ssids[0]), Ssid (ssids[1]) };

  //--- WiFi Manager ---
  wifi.SetRemoteStationManager (wifiManager,
                                "RtsCtsThreshold", UintegerValue (rtsThreshold));

  //--- Install Devices ---
  NetDeviceContainer apDevices, staDevices;
  for (uint32_t i = 0; i < nAps; ++i)
    {
      // AP
      WifiMacHelper mac;
      mac.SetType ("ns3::ApWifiMac", "Ssid", SsidValue (ssidObjs[i]));
      NetDeviceContainer apDev = wifi.Install (phy, mac, apNodes.Get (i));
      apDevices.Add (apDev);

      // STA
      mac.SetType ("ns3::StaWifiMac", "Ssid", SsidValue (ssidObjs[i]));
      NetDeviceContainer staDev = wifi.Install (phy, mac, staNodes.Get (i));
      staDevices.Add (staDev);
    }

  //--- Internet Stack/IP ---
  InternetStackHelper internet;
  internet.Install (apNodes);
  internet.Install (staNodes);

  Ipv4AddressHelper ipv4;
  std::vector<Ipv4InterfaceContainer> apIfaces, staIfaces;
  for (uint32_t i = 0; i < nAps; ++i)
    {
      std::ostringstream subnet;
      subnet << "10.1." << i+1 << ".0";
      ipv4.SetBase (subnet.str ().c_str (), "255.255.255.0");
      Ipv4InterfaceContainer iface = ipv4.Assign (
          NetDeviceContainer (apDevices.Get (i), staDevices.Get (i)));
      apIfaces.push_back (Ipv4InterfaceContainer (iface.Get (0)));
      staIfaces.push_back (Ipv4InterfaceContainer (iface.Get (1)));
    }

  //--- Applications: OnOff (AP->STA), PacketSink (STA) ---
  uint16_t basePort = 5000;
  std::vector<ApplicationContainer> sinks(nStas), onOffs(nAps);
  for (uint32_t i = 0; i < nAps; ++i)
    {
      // Sink on STA
      PacketSinkHelper sinkHelper ("ns3::UdpSocketFactory",
               InetSocketAddress (Ipv4Address::GetAny (), basePort + i));
      sinks[i] = sinkHelper.Install (staNodes.Get (i));
      sinks[i].Start (Seconds (0.0));
      sinks[i].Stop (Seconds (simTime+1));

      // OnOff on AP
      OnOffHelper onoff ("ns3::UdpSocketFactory",
               InetSocketAddress (staIfaces[i].GetAddress (0), basePort + i));
      onoff.SetAttribute ("DataRate", StringValue ("54Mbps"));
      onoff.SetAttribute ("PacketSize", UintegerValue (1472));
      onoff.SetAttribute ("StartTime", TimeValue (Seconds (1.0)));
      onoff.SetAttribute ("StopTime", TimeValue (Seconds (simTime)));
      onOffs[i].Add (onoff.Install (apNodes.Get (i)));
    }

  //--- Statistics ---
  Ptr<SimulationStats> simStats = CreateObject<SimulationStats> (nStas);
  simStats->SetDuration (simTime);
  // Attach traces: Phy, Rx, TxPower
  for (uint32_t i = 0; i < nStas; ++i)
    {
      Ptr<NetDevice> staDev = staDevices.Get (i);
      Ptr<WifiNetDevice> wifiDev = DynamicCast<WifiNetDevice> (staDev);
      Ptr<WifiPhy> wifiPhy = wifiDev->GetPhy ();
      AttachPhyStateTrace (wifiPhy, simStats->Get (i));
      AttachRxTrace (sinks[i].Get (0), simStats->Get (i));
    }
  // For transmit power, also connect AP NetDevices
  for (uint32_t i = 0; i < nAps; ++i)
    {
      Ptr<WifiNetDevice> wifiDev = DynamicCast<WifiNetDevice> (apDevices.Get (i));
      Ptr<WifiPhy> wifiPhy = wifiDev->GetPhy ();
      AttachPhyStateTrace (wifiPhy, simStats->Get (i));
    }

  simStats->ScheduleNext (Seconds (interval));

  //--- FlowMonitor ---
  FlowMonitorHelper flowmonHelper;
  Ptr<FlowMonitor> flowmon = flowmonHelper.InstallAll ();

  //--- Run ---
  Simulator::Stop (Seconds (simTime+0.1));
  Simulator::Run ();

  //--- End-of-simulation: Gather, plot, report ---
  simStats->Record (simTime);

  // Print per-flow statistics
  flowmon->CheckForLostPackets ();
  FlowMonitor::FlowStatsContainer stats = flowmon->GetFlowStats ();
  std::map<uint32_t, FlowMonitor::FlowStats> flowStatsMap (stats.begin (), stats.end ());
  std::cout << "\n=== FlowMonitor results ===\n";
  for (auto &flowPair : flowStatsMap)
    {
      FlowId flowId = flowPair.first;
      const FlowMonitor::FlowStats &st = flowPair.second;
      Ipv4FlowClassifier::FiveTuple fiveTuple =
          flowmonHelper.GetClassifier ()->FindFlow (flowId);

      // Only show AP->STA flows (should be all)
      double throughput = st.rxBytes * 8.0 / 1.0e6 / (st.timeLastRxPacket.GetSeconds () - st.timeFirstTxPacket.GetSeconds ());
      std::cout << "Flow " << flowId << " (" << fiveTuple.sourceAddress << " -> " << fiveTuple.destinationAddress << ")\n";
      std::cout << "  Tx Packets: " << st.txPackets << "\n";
      std::cout << "  Rx Packets: " << st.rxPackets << "\n";
      std::cout << "  Lost Packets: " << st.lostPackets << "\n";
      std::cout << "  Throughput: " << throughput << " Mbps\n";
      std::cout << "  Mean delay: " << (st.rxPackets ? st.delaySum.GetSeconds ()*1e3/st.rxPackets : 0) << " ms\n";
      std::cout << "  Mean jitter: " << (st.rxPackets ? st.jitterSum.GetSeconds ()*1e3/st.rxPackets : 0) << " ms\n";
    }

  //--- Gnuplot Generation ---
  uint32_t nPoints = (uint32_t) (simTime / interval);
  std::vector<double> timeVec;
  for (uint32_t i = 1; i <= nPoints; ++i)
    timeVec.push_back (i * interval);

  // Throughput plot
  std::vector< std::vector<double> > yThroughput (nStas);
  for (uint32_t i = 0; i < nStas; ++i)
    yThroughput[i] = simStats->GetThroughputs (i);
  WriteGnuplot ("throughput.png", "STA Throughput (Mbps)", timeVec, yThroughput, ssids);

  // Tx power plot
  std::vector< std::vector<double> > yPower (nStas);
  for (uint32_t i = 0; i < nStas; ++i)
    yPower[i] = simStats->GetPowers (i);
  WriteGnuplot ("txpower.png", "STA Average Tx Power (dBm)", timeVec, yPower, ssids);

  // WiFi state times plots
  for (uint32_t st = 0; st < WIFI_STATES; ++st)
    {
      std::vector< std::vector<double> > yState (nStas);
      for (uint32_t i = 0; i < nStas; ++i)
        yState[i] = simStats->GetStateTimes (i, st);
      std::string fname = std::string ("state_") + wifiStateNames[st] + ".png";
      std::string tstr = std::string ("Time in ") + wifiStateNames[st] + " (s)";
      WriteGnuplot (fname, tstr, timeVec, yState, ssids);
    }

  Simulator::Destroy ();
  return 0;
}