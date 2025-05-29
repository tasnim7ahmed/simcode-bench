#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/dsdv-helper.h"
#include "ns3/aodv-helper.h"
#include "ns3/olsr-helper.h"
#include "ns3/dsr-helper.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-helper.h"
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <random>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("ManetRoutingComparison");

enum RoutingProtocolType
{
  DSDV,
  AODV,
  OLSR,
  DSR
};

std::string RoutingProtocolTypeToString (RoutingProtocolType proto)
{
  switch (proto)
    {
    case DSDV:
      return "DSDV";
    case AODV:
      return "AODV";
    case OLSR:
      return "OLSR";
    case DSR:
      return "DSR";
    default:
      return "UNKNOWN";
    }
}

struct SimulationConfig
{
  RoutingProtocolType protocol = AODV;
  uint32_t numNodes = 50;
  double simulationTime = 200.0;
  double nodeSpeed = 20.0; // m/s
  uint32_t areaWidth = 1500;
  uint32_t areaHeight = 300;
  double txPower = 7.5; // dBm
  uint32_t numUdpPairs = 10;
  bool enableMobilityTracing = false;
  bool enableFlowMonitor = false;
  std::string csvFileName = "manet-stats.csv";
};

// Generate random unique node pairs
std::vector<std::pair<uint32_t, uint32_t>>
GenerateSourceSinkPairs (uint32_t numPairs, uint32_t numNodes)
{
  std::vector<uint32_t> nodes (numNodes);
  std::iota (nodes.begin (), nodes.end (), 0);
  std::vector<std::pair<uint32_t, uint32_t>> pairs;
  std::mt19937 rng (SeedManager::GetSeed () + SeedManager::GetRun ());
  std::uniform_int_distribution<uint32_t> dist (0, numNodes - 1);

  while (pairs.size () < numPairs)
    {
      uint32_t src = dist (rng);
      uint32_t dst = dist (rng);
      if (src != dst &&
          std::find_if (pairs.begin (), pairs.end (),
                        [src, dst] (const std::pair<uint32_t, uint32_t> &p)
                        { return (p.first == src && p.second == dst); }) == pairs.end ())
        {
          pairs.emplace_back (src, dst);
        }
    }
  return pairs;
}

void
ParseArgs (int argc, char **argv, SimulationConfig &cfg)
{
  CommandLine cmd;
  std::string protoStr = "AODV";
  cmd.AddValue ("protocol", "Routing protocol: DSDV|AODV|OLSR|DSR", protoStr);
  cmd.AddValue ("numNodes", "Number of nodes", cfg.numNodes);
  cmd.AddValue ("simulationTime", "Simulation time (seconds)", cfg.simulationTime);
  cmd.AddValue ("nodeSpeed", "Node speed (m/s)", cfg.nodeSpeed);
  cmd.AddValue ("areaWidth", "Simulation area width (meters)", cfg.areaWidth);
  cmd.AddValue ("areaHeight", "Simulation area height (meters)", cfg.areaHeight);
  cmd.AddValue ("txPower", "WiFi TX power (dBm)", cfg.txPower);
  cmd.AddValue ("numUdpPairs", "Number of UDP source/sink pairs", cfg.numUdpPairs);
  cmd.AddValue ("traceMobility", "Enable mobility tracing", cfg.enableMobilityTracing);
  cmd.AddValue ("enableFlowMonitor", "Enable FlowMonitor", cfg.enableFlowMonitor);
  cmd.AddValue ("outCsv", "CSV output file", cfg.csvFileName);
  cmd.Parse (argc, argv);

  std::transform (protoStr.begin (), protoStr.end (), protoStr.begin (), ::toupper);
  if (protoStr == "DSDV")
    cfg.protocol = DSDV;
  else if (protoStr == "AODV")
    cfg.protocol = AODV;
  else if (protoStr == "OLSR")
    cfg.protocol = OLSR;
  else if (protoStr == "DSR")
    cfg.protocol = DSR;
  else
    {
      NS_ABORT_MSG ("Unknown routing protocol " << protoStr);
    }
}

int
main (int argc, char **argv)
{
  SimulationConfig cfg;
  ParseArgs (argc, argv, cfg);

  SeedManager::SetSeed (12345);

  NodeContainer nodes;
  nodes.Create (cfg.numNodes);

  // Mobility
  MobilityHelper mobility;
  mobility.SetMobilityModel ("ns3::RandomWaypointMobilityModel",
                             "Speed", StringValue ("ns3::ConstantRandomVariable[Constant=" + std::to_string (cfg.nodeSpeed)+"]"),
                             "Pause", StringValue ("ns3::ConstantRandomVariable[Constant=0.0]"),
                             "PositionAllocator",
                                PointerValue (CreateObjectWithAttributes<RandomRectanglePositionAllocator> (
                                    "X", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=" + std::to_string (cfg.areaWidth) + "]"),
                                    "Y", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=" + std::to_string (cfg.areaHeight) + "]"))));
  mobility.Install (nodes);

  if (cfg.enableMobilityTracing)
    {
      mobility.EnableAsciiAll (std::ofstream ("mobility-trace.tr"));
    }

  // WiFi Config
  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211b);

  WifiMacHelper wifiMac;
  wifiMac.SetType ("ns3::AdhocWifiMac");

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
  wifiPhy.SetChannel (wifiChannel.Create ());

  wifiPhy.Set ("TxPowerStart", DoubleValue (cfg.txPower));
  wifiPhy.Set ("TxPowerEnd", DoubleValue (cfg.txPower));
  wifiPhy.Set ("EnergyDetectionThreshold", DoubleValue (-96.0));
  wifiPhy.Set ("RxGain", DoubleValue (0));
  wifiPhy.Set ("TxGain", DoubleValue (0));
  wifiPhy.Set ("CcaMode1Threshold", DoubleValue (-99.0));

  NetDeviceContainer devices = wifi.Install (wifiPhy, wifiMac, nodes);

  // Internet + Routing
  InternetStackHelper internet;
  Ipv4ListRoutingHelper list;

  if (cfg.protocol == DSDV)
    {
      DsdvHelper proto;
      list.Add (proto, 10);
    }
  else if (cfg.protocol == AODV)
    {
      AodvHelper proto;
      list.Add (proto, 10);
    }
  else if (cfg.protocol == OLSR)
    {
      OlsrHelper proto;
      list.Add (proto, 10);
    }
  else if (cfg.protocol == DSR)
    {
      // DSR special: non-list-based installation
      DsrMainHelper dsrMain;
      DsrHelper dsr;
      internet.Install (nodes);
      dsrMain.Install (dsr, nodes);
    }
  if (cfg.protocol != DSR)
    {
      internet.SetRoutingHelper (list);
      internet.Install (nodes);
    }

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.0.0", "255.255.0.0");
  Ipv4InterfaceContainer interfaces = ipv4.Assign (devices);

  // UDP Applications
  uint16_t port = 8000;
  std::vector<ApplicationContainer> serverApps, clientApps;
  std::vector<std::pair<uint32_t, uint32_t>> udpPairs = GenerateSourceSinkPairs (cfg.numUdpPairs, cfg.numNodes);

  std::vector<double> randomStartTimes;
  std::uniform_real_distribution<double> cstart (50.0, 51.0);
  std::mt19937 gen (SeedManager::GetSeed () + SeedManager::GetRun () + 1);
  for (uint32_t i = 0; i < cfg.numUdpPairs; ++i)
    randomStartTimes.push_back (cstart (gen));

  for (uint32_t i = 0; i < cfg.numUdpPairs; ++i)
    {
      uint32_t srcIdx = udpPairs[i].first;
      uint32_t dstIdx = udpPairs[i].second;

      // UDP Server
      UdpServerHelper server (port + i);
      ApplicationContainer serverApp = server.Install (nodes.Get (dstIdx));
      serverApp.Start (Seconds (0.0));
      serverApp.Stop (Seconds (cfg.simulationTime));
      serverApps.push_back (serverApp);

      // UDP Client
      uint32_t maxPackets = 1000000;
      double interval = 0.05;
      uint32_t packetSize = 512;

      UdpClientHelper client (interfaces.GetAddress (dstIdx), port + i);
      client.SetAttribute ("MaxPackets", UintegerValue (maxPackets));
      client.SetAttribute ("Interval", TimeValue (Seconds (interval)));
      client.SetAttribute ("PacketSize", UintegerValue (packetSize));
      ApplicationContainer clientApp = client.Install (nodes.Get (srcIdx));
      clientApp.Start (Seconds (randomStartTimes[i]));
      clientApp.Stop (Seconds (cfg.simulationTime));
      clientApps.push_back (clientApp);
    }

  // FlowMonitor if needed
  Ptr<FlowMonitor> flowMonitor;
  Ptr<FlowMonitorHelper> flowHelper;
  if (cfg.enableFlowMonitor)
    {
      flowHelper = CreateObject<FlowMonitorHelper> ();
      flowMonitor = flowHelper->InstallAll ();
    }

  Simulator::Stop (Seconds (cfg.simulationTime));
  Simulator::Run ();

  // Collect results
  std::ofstream csv (cfg.csvFileName, std::ios_base::out);
  csv << "protocol,numNodes,udpPair,srcNode,dstNode,packetsReceived,bytesReceived,simulationTime\n";
  for (uint32_t i = 0; i < cfg.numUdpPairs; ++i)
    {
      uint32_t dstIdx = udpPairs[i].second;
      Ptr<UdpServer> server = DynamicCast<UdpServer> (serverApps[i].Get (0));
      uint64_t totalPackets = server->GetReceived ();
      uint64_t totalBytes = server->GetReceivedBytes ();
      csv << RoutingProtocolTypeToString (cfg.protocol) << ","
          << cfg.numNodes << ","
          << i << ","
          << udpPairs[i].first << ","
          << dstIdx << ","
          << totalPackets << ","
          << totalBytes << ","
          << cfg.simulationTime << "\n";
    }
  csv.close ();

  if (cfg.enableFlowMonitor)
    {
      flowMonitor->SerializeToXmlFile ("manet-flows.xml", true, true);
    }

  Simulator::Destroy ();
  return 0;
}