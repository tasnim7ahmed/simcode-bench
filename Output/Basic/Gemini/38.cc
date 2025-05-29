#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/gnuplot.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <string>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiSimulation");

class NodeStatistics {
public:
  NodeStatistics (NodeContainer nodes, std::string prefix);
  void Start (void);
  void Stop (void);
  void Report (void);

private:
  void MonitorRx (std::string path, Time rxStart, Time rxEnd, uint32_t packetSize);
  void MonitorTx (std::string path, Time txStart, Time txEnd);
  void MonitorCcBusy (std::string path, Time ccBusyStart, Time ccBusyEnd);
  void MonitorIdle (std::string path, Time idleStart, Time idleEnd);
  void PacketReceived (Ptr<const Packet> p, const Address &addr);
  void AverageTxPower (std::string path, double txPowerDbm);

  NodeContainer m_nodes;
  std::string m_prefix;
  EventId m_reportEvent;
  std::vector<Time> m_ccaBusyTimes;
  std::vector<Time> m_idleTimes;
  std::vector<Time> m_txTimes;
  std::vector<Time> m_rxTimes;
  uint64_t m_totalBytesReceived;
  double m_averageTxPower;
  uint32_t m_txCount;
  std::map<uint32_t, Ptr<FlowMonitor>> m_flowMonitors;
};

NodeStatistics::NodeStatistics (NodeContainer nodes, std::string prefix)
  : m_nodes (nodes),
    m_prefix (prefix),
    m_totalBytesReceived (0),
    m_averageTxPower (0.0),
    m_txCount (0)
{
  m_ccaBusyTimes.resize (m_nodes.GetN ());
  m_idleTimes.resize (m_nodes.GetN ());
  m_txTimes.resize (m_nodes.GetN ());
  m_rxTimes.resize (m_nodes.GetN ());

  for (uint32_t i = 0; i < m_nodes.GetN (); ++i)
    {
      m_ccaBusyTimes[i] = Seconds (0.0);
      m_idleTimes[i] = Seconds (0.0);
      m_txTimes[i] = Seconds (0.0);
      m_rxTimes[i] = Seconds (0.0);
    }
}

void
NodeStatistics::Start (void)
{
  for (uint32_t i = 0; i < m_nodes.GetN (); ++i)
    {
      std::ostringstream oss;
      oss << "/NodeList/" << m_nodes.GetId (i) << "/DeviceList/*/$ns3::WifiNetDevice/Mac/EdcaTxopN/";
      oss << "m_channelAssessBusyTime";
      std::string path = oss.str ();
      Config::Connect (path, MakeBoundCallback (&NodeStatistics::MonitorCcBusy, this));

      oss.str ("");
      oss << "/NodeList/" << m_nodes.GetId (i) << "/DeviceList/*/$ns3::WifiNetDevice/Mac/EdcaTxopN/";
      oss << "m_idleTime";
      path = oss.str ();
      Config::Connect (path, MakeBoundCallback (&NodeStatistics::MonitorIdle, this));

      oss.str ("");
      oss << "/NodeList/" << m_nodes.GetId (i) << "/DeviceList/*/$ns3::WifiNetDevice/Mac/MacTx/";
      oss << "MacTxBegin";
      path = oss.str ();
      Config::Connect (path, MakeBoundCallback (&NodeStatistics::MonitorTx, this));

      oss.str ("");
      oss << "/NodeList/" << m_nodes.GetId (i) << "/ApplicationList/*/$ns3::PacketSink/Rx";
      path = oss.str ();
      Config::ConnectWithoutContext (path, MakeCallback (&NodeStatistics::PacketReceived, this));

      oss.str ("");
      oss << "/NodeList/" << m_nodes.GetId (i) << "/DeviceList/*/$ns3::WifiNetDevice/Phy/TxPowerDbm";
      path = oss.str ();
      Config::Connect (path, MakeBoundCallback (&NodeStatistics::AverageTxPower, this));
    }

  m_reportEvent = Simulator::Schedule (Seconds (1.0), &NodeStatistics::Report, this);
}

void
NodeStatistics::Stop (void)
{
  Simulator::Cancel (m_reportEvent);
}

void
NodeStatistics::Report (void)
{
  std::cout << "--- Statistics Report ---" << std::endl;
  double totalThroughput = 0.0;

  for (uint32_t i = 0; i < m_nodes.GetN (); ++i)
    {
      double busyTime = m_ccaBusyTimes[i].GetSeconds ();
      double idleTime = m_idleTimes[i].GetSeconds ();
      double txTime = m_txTimes[i].GetSeconds ();
      double rxTime = m_rxTimes[i].GetSeconds ();

      std::cout << "Node " << i << ":" << std::endl;
      std::cout << "  CCA Busy Time: " << busyTime << " s" << std::endl;
      std::cout << "  Idle Time: " << idleTime << " s" << std::endl;
      std::cout << "  Tx Time: " << txTime << " s" << std::endl;
      std::cout << "  Rx Time: " << rxTime << " s" << std::endl;

      double throughput = (m_totalBytesReceived * 8.0) / (Simulator::Now ().GetSeconds() * 1000000.0); // Mbps
      std::cout << "  Throughput: " << throughput << " Mbps" << std::endl;
      totalThroughput += throughput;
    }

  std::cout << "Total Throughput: " << totalThroughput << " Mbps" << std::endl;
  std::cout << "Average Transmit Power: " << (m_averageTxPower / m_txCount) << " dBm" << std::endl;

  m_reportEvent = Simulator::Schedule (Seconds (1.0), &NodeStatistics::Report, this);
}

void
NodeStatistics::MonitorRx (std::string path, Time rxStart, Time rxEnd, uint32_t packetSize)
{
  uint32_t nodeId = Simulator::GetContext ();
  m_rxTimes[nodeId] += (rxEnd - rxStart);
}

void
NodeStatistics::MonitorTx (std::string path, Time txStart, Time txEnd)
{
  uint32_t nodeId = Simulator::GetContext ();
  m_txTimes[nodeId] += (txEnd - txStart);
}

void
NodeStatistics::MonitorCcBusy (std::string path, Time ccBusyStart, Time ccBusyEnd)
{
  uint32_t nodeId = Simulator::GetContext ();
  m_ccaBusyTimes[nodeId] += (ccBusyEnd - ccBusyStart);
}

void
NodeStatistics::MonitorIdle (std::string path, Time idleStart, Time idleEnd)
{
  uint32_t nodeId = Simulator::GetContext ();
  m_idleTimes[nodeId] += (idleEnd - idleStart);
}

void
NodeStatistics::PacketReceived (Ptr<const Packet> p, const Address &addr)
{
  m_totalBytesReceived += p->GetSize ();
}

void
NodeStatistics::AverageTxPower (std::string path, double txPowerDbm)
{
  m_averageTxPower += txPowerDbm;
  m_txCount++;
}

int
main (int argc, char *argv[])
{
  bool verbose = false;
  uint32_t nAp = 2;
  uint32_t nSta = 2;
  std::string wifiManagerType = "ns3::ParfWifiManager";
  double simulationTime = 100.0;
  int rtsCtsThreshold = 2347;
  double txPowerDbm = 20.0;

  CommandLine cmd;
  cmd.AddValue ("verbose", "Tell echo applications to log if set to true", verbose);
  cmd.AddValue ("nAp", "Number of APs", nAp);
  cmd.AddValue ("nSta", "Number of STAs per AP", nSta);
  cmd.AddValue ("wifiManager", "Wifi Manager Type", wifiManagerType);
  cmd.AddValue ("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.AddValue ("rtsCtsThreshold", "RTS/CTS Threshold", rtsCtsThreshold);
  cmd.AddValue ("txPower", "Transmit Power (dBm)", txPowerDbm);
  cmd.Parse (argc, argv);

  Config::SetDefault ("ns3::WifiMac::Ssid", SsidValue (Ssid ("ns3-wifi")));

  NodeContainer apNodes;
  apNodes.Create (nAp);

  NodeContainer staNodes;
  staNodes.Create (nAp * nSta);

  WifiHelper wifiHelper;
  wifiHelper.SetStandard (WIFI_PHY_STANDARD_80211a);

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
  wifiPhy.SetChannel (wifiChannel.Create ());
  wifiPhy.Set ("TxPowerStart", DoubleValue (txPowerDbm));
  wifiPhy.Set ("TxPowerEnd", DoubleValue (txPowerDbm));

  Config::SetDefault ("ns3::WifiMac::RtsCtsThreshold", IntegerValue (rtsCtsThreshold));

  if (!wifiManagerType.empty ())
    {
      TypeId wifiManagerTypeId;
      if (TypeId::LookupByNameFailSafe (wifiManagerType, &wifiManagerTypeId))
        {
          Config::SetDefault ("ns3::WifiMac::WifiManager", TypeIdValue (wifiManagerTypeId));
        }
      else
        {
          std::cerr << "Error: WifiManager type " << wifiManagerType << " not found." << std::endl;
          return 1;
        }
    }

  WifiMacHelper macHelper;
  NetDeviceContainer apDevices;
  NetDeviceContainer staDevices;
  for (uint32_t i = 0; i < nAp; ++i)
    {
      Ssid ssid = Ssid (std::string ("ns3-wifi-") + std::to_string (i));
      macHelper.SetType ("ns3::ApWifiMac",
                         "Ssid", SsidValue (ssid));

      apDevices.Add (wifiHelper.Install (wifiPhy, macHelper, apNodes.Get (i)));

      macHelper.SetType ("ns3::StaWifiMac",
                         "Ssid", SsidValue (ssid),
                         "ActiveProbing", BooleanValue (false));
      NetDeviceContainer clientDevices = wifiHelper.Install (wifiPhy, macHelper, staNodes.Get (i * nSta), staNodes.Get (i * nSta + 1));
      staDevices.Add (clientDevices);
    }

  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::ListPositionAllocator");
  auto positionAlloc = mobility.GetPositionAllocator ()->GetObject<ListPositionAllocator> ();

  positionAlloc->Add (Vector (10.0, 10.0, 0.0));
  positionAlloc->Add (Vector (50.0, 10.0, 0.0));
  positionAlloc->Add (Vector (12.0, 20.0, 0.0));
  positionAlloc->Add (Vector (52.0, 20.0, 0.0));

  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (apNodes);
  mobility.Install (staNodes);

  InternetStackHelper internet;
  internet.Install (apNodes);
  internet.Install (staNodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer apInterfaces = ipv4.Assign (apDevices);

  ipv4.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer staInterfaces = ipv4.Assign (staDevices);

  for (uint32_t i = 0; i < nAp * nSta; i++)
  {
    Ptr<Node> staNode = staNodes.Get (i);
    Ptr<Ipv4> ipv4Sta = staNode->GetObject<Ipv4> ();
    Ipv4Address staAddress = ipv4Sta->GetAddress (1, 0).GetLocal ();
    Ipv4GlobalRoutingHelper::PopulateRoutingTable (staNode);
  }
  for (uint32_t i = 0; i < nAp; i++)
  {
    Ptr<Node> apNode = apNodes.Get (i);
    Ptr<Ipv4> ipv4Ap = apNode->GetObject<Ipv4> ();
    Ipv4Address apAddress = ipv4Ap->GetAddress (1, 0).GetLocal ();
    Ipv4GlobalRoutingHelper::PopulateRoutingTable (apNode);
  }

  uint16_t port = 9;
  OnOffHelper onoff ("ns3::UdpSocketFactory",
                     Address (InetSocketAddress (staInterfaces.GetAddress (0), port)));
  onoff.SetConstantRate (DataRate ("54Mbps"));
  ApplicationContainer appSource = onoff.Install (apNodes.Get (0));

  PacketSinkHelper sink ("ns3::UdpSocketFactory",
                         Address (InetSocketAddress (Ipv4Address::GetAny (), port)));
  ApplicationContainer appSink = sink.Install (staNodes.Get (0));

   OnOffHelper onoff2 ("ns3::UdpSocketFactory",
                     Address (InetSocketAddress (staInterfaces.GetAddress (2), port)));
  onoff2.SetConstantRate (DataRate ("54Mbps"));
  ApplicationContainer appSource2 = onoff2.Install (apNodes.Get (1));

  PacketSinkHelper sink2 ("ns3::UdpSocketFactory",
                         Address (InetSocketAddress (Ipv4Address::GetAny (), port)));
  ApplicationContainer appSink2 = sink2.Install (staNodes.Get (2));


  appSource.Start (Seconds (1.0));
  appSource.Stop (Seconds (simulationTime));

  appSink.Start (Seconds (1.0));
  appSink.Stop (Seconds (simulationTime));
  
  appSource2.Start (Seconds (1.0));
  appSource2.Stop (Seconds (simulationTime));

  appSink2.Start (Seconds (1.0));
  appSink2.Stop (Seconds (simulationTime));

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  NodeStatistics stats (staNodes, "wifi-example");
  stats.Start ();

  Simulator::Stop (Seconds (simulationTime));
  Simulator::Run ();

  monitor->CheckForLostPackets ();

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  FlowMonitor::FlowStatsContainer stats_cont = monitor->GetFlowStats ();

  for (auto i = stats_cont.begin (); i != stats_cont.end (); ++i)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
      std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
      std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
      std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
      std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds () - i->second.timeFirstTxPacket.GetSeconds ()) / 1000000 << " Mbps\n";
      std::cout << "  Mean delay:   " << i->second.delaySum.GetSeconds () / i->second.rxPackets << " s\n";
      std::cout << "  Mean jitter:  " << i->second.jitterSum.GetSeconds () / (i->second.rxPackets - 1) << " s\n";
    }

  monitor->SerializeToXmlFile ("wifi-example.flowmon", true, true);
  stats.Stop ();

  Simulator::Destroy ();
  return 0;
}