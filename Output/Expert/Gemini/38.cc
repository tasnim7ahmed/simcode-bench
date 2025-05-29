#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"
#include "ns3/gnuplot.h"
#include "ns3/command-line.h"
#include "ns3/propagation-loss-model.h"
#include "ns3/propagation-delay-model.h"

#include <iostream>
#include <fstream>
#include <string>
#include <vector>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiSimulation");

class NodeStatistics
{
public:
  NodeStatistics (Ptr<Node> node, Time interval) :
      m_node (node),
      m_interval (interval),
      m_bytesReceived (0),
      m_totalTxPower (0),
      m_txSamples (0),
      m_lastRx (Seconds (0))
  {
    m_wifiPhy = m_node->GetDevice (0)->GetObject<WifiNetDevice> ()->GetPhy ();
    m_wifiMac = m_node->GetDevice (0)->GetObject<WifiNetDevice> ()->GetMac ();

    m_channel = m_wifiPhy->GetChannel ();
    m_wifiChannelRxCallback = MakeCallback (&NodeStatistics::ChannelRxTrace, this);
    m_channel->TraceConnectWithoutContext ("RxOk", m_wifiChannelRxCallback);

    m_wifiPhyTxCallback = MakeCallback (&NodeStatistics::PhyTxTrace, this);
    m_wifiPhy->TraceConnectWithoutContext ("StartTx", m_wifiPhyTxCallback);

    m_wifiMacPromiscSnifferCallback = MakeCallback (&NodeStatistics::PromiscSnifferTrace, this);
    m_wifiMac->TraceConnectWithoutContext ("PromiscSnifferRx", m_wifiMacPromiscSnifferCallback);
    m_wifiMac->TraceConnectWithoutContext ("PromiscSnifferTx", m_wifiMacPromiscSnifferCallback);

    m_totalTime = Seconds (0);
    m_ccaBusyTime = Seconds (0);
    m_txTime = Seconds (0);
    m_rxTime = Seconds (0);
    m_idleTime = Seconds (0);

    Simulator::Schedule (m_interval, &NodeStatistics::Report, this);
  }

  void EnableGnuplot (std::string filenameBase)
  {
    m_gnuplotFilenameBase = filenameBase;
    m_gnuplot = true;
  }

private:
  void PhyTxTrace (Ptr<const Packet> packet, WifiMode mode, uint16_t preambleDsssPlcpLength, uint8_t plcpHeaderLength, WifiTxVector txVector)
  {
    m_totalTxPower += txVector.GetTxPowerLevel ();
    m_txSamples++;
  }

  void ChannelRxTrace (Ptr<const Packet> packet, double snr, WifiMode mode, enum WifiPreamble preamble)
  {
    m_bytesReceived += packet->GetSize ();
    Time now = Simulator::Now ();
    m_rxTime += now - m_lastRx;
    m_lastRx = now;
  }

  void PromiscSnifferTrace (Ptr<const Packet> packet, Mac48Address from, Mac48Address to, WifiMode mode, enum WifiPreamble preamble, uint8_t channelNumber, int16_t rssi, uint8_t rate)
  {
    Time now = Simulator::Now ();
    if (to == Mac48Address::GetBroadcast ())
    {
      m_ccaBusyTime += now - m_lastRx;
    }

    m_lastRx = now;
  }

  void Report ()
  {
    Time now = Simulator::Now ();
    Time interval = now - m_totalTime;
    m_totalTime = now;

    m_idleTime = interval - m_ccaBusyTime - m_rxTime - m_txTime;

    double throughput = (double) m_bytesReceived * 8 / interval.ToDouble (Time::S);
    double avgTxPower = m_txSamples > 0 ? m_totalTxPower / m_txSamples : 0;

    NS_LOG_UNCOND ("Time: " << now.ToDouble (Time::S)
                            << " Throughput: " << throughput
                            << " Avg Tx Power: " << avgTxPower
                            << " CCA Busy Time: " << m_ccaBusyTime.ToDouble (Time::S)
                            << " Idle Time: " << m_idleTime.ToDouble (Time::S)
                            << " Tx Time: " << m_txTime.ToDouble (Time::S)
                            << " Rx Time: " << m_rxTime.ToDouble (Time::S));

    if (m_gnuplot)
    {
      WriteGnuplotFile (m_gnuplotFilenameBase + ".dat", now.ToDouble (Time::S), throughput, avgTxPower, m_ccaBusyTime.ToDouble (Time::S), m_idleTime.ToDouble (Time::S), m_txTime.ToDouble (Time::S), m_rxTime.ToDouble (Time::S));
      CreateGnuplotScript (m_gnuplotFilenameBase + ".plt", m_gnuplotFilenameBase + ".dat", m_gnuplotFilenameBase + ".png");
    }

    m_bytesReceived = 0;
    m_totalTxPower = 0;
    m_txSamples = 0;
    m_ccaBusyTime = Seconds (0);
    m_txTime = Seconds (0);
    m_rxTime = Seconds (0);

    Simulator::Schedule (m_interval, &NodeStatistics::Report, this);
  }

  void WriteGnuplotFile (std::string filename, double time, double throughput, double avgTxPower, double ccaBusyTime, double idleTime, double txTime, double rxTime)
  {
    std::ofstream outFile (filename.c_str (), std::ios::app);
    outFile << time << " " << throughput << " " << avgTxPower << " " << ccaBusyTime << " " << idleTime << " " << txTime << " " << rxTime << std::endl;
  }

  void CreateGnuplotScript (std::string filename, std::string dataFilename, std::string outputFilename)
  {
    std::ofstream outFile (filename.c_str ());
    outFile << "set terminal png size 1024,768" << std::endl;
    outFile << "set output \"" << outputFilename << "\"" << std::endl;
    outFile << "set title \"Wifi Performance\"" << std::endl;
    outFile << "set xlabel \"Time (s)\"" << std::endl;

    outFile << "set multiplot layout 3,1" << std::endl;

    outFile << "set ylabel \"Throughput (bps)\"" << std::endl;
    outFile << "plot \"" << dataFilename << "\" using 1:2 with lines title \"Throughput\"" << std::endl;

    outFile << "set ylabel \"Avg Tx Power\"" << std::endl;
    outFile << "plot \"" << dataFilename << "\" using 1:3 with lines title \"Avg Tx Power\"" << std::endl;

    outFile << "set ylabel \"Time (s)\"" << std::endl;
    outFile << "plot \"" << dataFilename << "\" using 1:4 with lines title \"CCA Busy\", \\\n"
            << "\"" << dataFilename << "\" using 1:5 with lines title \"Idle\", \\\n"
            << "\"" << dataFilename << "\" using 1:6 with lines title \"Tx\", \\\n"
            << "\"" << dataFilename << "\" using 1:7 with lines title \"Rx\"" << std::endl;

    outFile << "unset multiplot" << std::endl;
  }

  Ptr<Node> m_node;
  Time m_interval;
  uint64_t m_bytesReceived;
  double m_totalTxPower;
  uint32_t m_txSamples;
  Time m_lastRx;

  Ptr<WifiPhy> m_wifiPhy;
  Ptr<WifiMac> m_wifiMac;
  Ptr<YansWifiChannel> m_channel;
  Callback<void, Ptr<const Packet>, double, WifiMode, enum WifiPreamble> m_wifiChannelRxCallback;
  Callback<void, Ptr<const Packet>, WifiMode, uint16_t, uint8_t, WifiTxVector> m_wifiPhyTxCallback;
  Callback<void, Ptr<const Packet>, Mac48Address, Mac48Address, WifiMode, enum WifiPreamble, uint8_t, int16_t, uint8_t> m_wifiMacPromiscSnifferCallback;

  Time m_totalTime;
  Time m_ccaBusyTime;
  Time m_txTime;
  Time m_rxTime;
  Time m_idleTime;

  bool m_gnuplot = false;
  std::string m_gnuplotFilenameBase;
};

int main (int argc, char *argv[])
{
  bool verbose = false;
  uint32_t nAp = 2;
  uint32_t nSta = 2;
  std::string wifiManagerType = "ns3::ParfWifiManager";
  uint32_t rtsCtsThreshold = 2200;
  double txPowerLevel = 16.0206;
  double simulationTime = 100;
  bool enableGnuplot = true;

  CommandLine cmd;
  cmd.AddValue ("verbose", "Tell application to log if true.", verbose);
  cmd.AddValue ("nAp", "Number of AP nodes", nAp);
  cmd.AddValue ("nSta", "Number of STA nodes", nSta);
  cmd.AddValue ("wifiManagerType", "Wifi Manager Type", wifiManagerType);
  cmd.AddValue ("rtsCtsThreshold", "RTS/CTS threshold", rtsCtsThreshold);
  cmd.AddValue ("txPowerLevel", "Tx Power Level", txPowerLevel);
  cmd.AddValue ("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.AddValue ("enableGnuplot", "Enable Gnuplot output", enableGnuplot);
  cmd.Parse (argc, argv);

  if (verbose)
    {
      LogComponentEnable ("WifiSimulation", LOG_LEVEL_INFO);
    }

  Config::SetDefault ("ns3::WifiRemoteStationManager::RtsCtsThreshold", UintegerValue (rtsCtsThreshold));

  NodeContainer apNodes;
  apNodes.Create (nAp);

  NodeContainer staNodes;
  staNodes.Create (nSta);

  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211a);
  wifi.SetRemoteStationManager (wifiManagerType, "TxPowerStart", DoubleValue (txPowerLevel), "TxPowerEnd", DoubleValue (txPowerLevel));

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
  wifiChannel.AddPropagationLoss ("ns3::FriisPropagationLossModel");
  wifiChannel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
  wifiPhy.SetChannel (wifiChannel.Create ());

  WifiMacHelper mac;
  Ssid ssid1 = Ssid ("ns3-ssid1");
  Ssid ssid2 = Ssid ("ns3-ssid2");

  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid1));
  NetDeviceContainer apDevice1 = wifi.Install (wifiPhy, mac, apNodes.Get (0));

  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid2));
  NetDeviceContainer apDevice2 = wifi.Install (wifiPhy, mac, apNodes.Get (1));

  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid1),
               "ActiveProbing", BooleanValue (false));
  NetDeviceContainer staDevice1 = wifi.Install (wifiPhy, mac, staNodes.Get (0));

  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid2),
               "ActiveProbing", BooleanValue (false));
  NetDeviceContainer staDevice2 = wifi.Install (wifiPhy, mac, staNodes.Get (1));

  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::ListPositionAllocator");
  Ptr<ListPositionAllocator> positionAlloc = DynamicCast<ListPositionAllocator> (mobility.GetPositionAllocator ());

  positionAlloc->Add (Vector (10.0, 10.0, 0.0));
  positionAlloc->Add (Vector (40.0, 10.0, 0.0));
  positionAlloc->Add (Vector (10.0, 20.0, 0.0));
  positionAlloc->Add (Vector (40.0, 20.0, 0.0));

  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (apNodes);
  mobility.Install (staNodes);

  InternetStackHelper internet;
  internet.Install (apNodes);
  internet.Install (staNodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer apInterface1 = ipv4.Assign (apDevice1);
  Ipv4InterfaceContainer staInterface1 = ipv4.Assign (staDevice1);

  ipv4.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer apInterface2 = ipv4.Assign (apDevice2);
  Ipv4InterfaceContainer staInterface2 = ipv4.Assign (staDevice2);

  UdpEchoServerHelper echoServer (9);
  ApplicationContainer serverApps1 = echoServer.Install (staNodes.Get (0));
  serverApps1.Start (Seconds (1.0));
  serverApps1.Stop (Seconds (simulationTime));

  ApplicationContainer serverApps2 = echoServer.Install (staNodes.Get (1));
  serverApps2.Start (Seconds (1.0));
  serverApps2.Stop (Seconds (simulationTime));

  OnOffHelper onoff1 ("ns3::UdpSocketFactory",
                       Address (InetSocketAddress (staInterface1.GetAddress (0), 9)));
  onoff1.SetConstantRate (DataRate ("54Mbps"));
  ApplicationContainer clientApps1 = onoff1.Install (apNodes.Get (0));
  clientApps1.Start (Seconds (2.0));
  clientApps1.Stop (Seconds (simulationTime));

  OnOffHelper onoff2 ("ns3::UdpSocketFactory",
                       Address (InetSocketAddress (staInterface2.GetAddress (0), 9)));
  onoff2.SetConstantRate (DataRate ("54Mbps"));
  ApplicationContainer clientApps2 = onoff2.Install (apNodes.Get (1));
  clientApps2.Start (Seconds (2.0));
  clientApps2.Stop (Seconds (simulationTime));

  PacketSinkHelper sink1 ("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), 9));
  ApplicationContainer sinkApps1 = sink1.Install (staNodes.Get (0));
  sinkApps1.Start (Seconds (0.0));
  sinkApps1.Stop (Seconds (simulationTime));

  PacketSinkHelper sink2 ("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), 9));
  ApplicationContainer sinkApps2 = sink2.Install (staNodes.Get (1));
  sinkApps2.Start (Seconds (0.0));
  sinkApps2.Stop (Seconds (simulationTime));

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  NodeStatistics stats1 (staNodes.Get (0), Seconds (1.0));
  NodeStatistics stats2 (staNodes.Get (1), Seconds (1.0));

  if (enableGnuplot)
  {
    stats1.EnableGnuplot ("sta1");
    stats2.EnableGnuplot ("sta2");
  }

  Simulator::Stop (Seconds (simulationTime));

  AnimationInterface anim ("wifi-animation.xml");
  anim.SetConstantPosition (apNodes.Get (0), 10, 10);
  anim.SetConstantPosition (apNodes.Get (1), 40, 10);
  anim.SetConstantPosition (staNodes.Get (0), 10, 20);
  anim.SetConstantPosition (staNodes.Get (1), 40, 20);

  Simulator::Run ();

  monitor->CheckForLostPackets ();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats ();
  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
      std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
      std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
      std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
      std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds () - i->second.timeFirstTxPacket.GetSeconds ()) / 1024 / 1024  << " Mbps\n";
      std::cout << "  Delay:      " << i->second.delaySum.GetSeconds () / i->second.rxPackets << " s\n";
      std::cout << "  Jitter:     " << i->second.jitterSum.GetSeconds () / (i->second.rxPackets - 1) << " s\n";
    }

  Simulator::Destroy ();
  return 0;
}