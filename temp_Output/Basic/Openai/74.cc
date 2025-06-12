#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/olsr-helper.h"
#include "ns3/aodv-helper.h"
#include "ns3/dsdv-helper.h"
#include "ns3/dsr-helper.h"
#include "ns3/dsr-main-helper.h"
#include "ns3/flow-monitor-helper.h"
#include <fstream>
#include <iomanip>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("ManetRoutingComparison");

enum RoutingProtocolType
{
  DSDV,
  AODV,
  OLSR,
  DSR
};

std::string
RoutingProtocolTypeToString (RoutingProtocolType type)
{
  switch (type)
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

int main (int argc, char *argv[])
{
  uint32_t numNodes = 50;
  uint32_t numFlows = 10;
  double simTime = 200.0;
  double areaWidth = 1500;
  double areaHeight = 300;
  double nodeSpeed = 20;
  double txPowerDbm = 7.5;
  std::string routingProtocolStr = "AODV";
  bool enableMobilityTrace = false;
  bool enableFlowMonitor = false;
  std::string csvFileName = "manet-comparison.csv";
  RoutingProtocolType routingProtocol = AODV;

  CommandLine cmd;
  cmd.AddValue ("nodes", "Number of nodes", numNodes);
  cmd.AddValue ("flows", "Number of UDP source/dest pairs", numFlows);
  cmd.AddValue ("simTime", "Simulation time (s)", simTime);
  cmd.AddValue ("width", "Area width (m)", areaWidth);
  cmd.AddValue ("height", "Area height (m)", areaHeight);
  cmd.AddValue ("speed", "Node speed (m/s)", nodeSpeed);
  cmd.AddValue ("txPower", "Tx power in dBm", txPowerDbm);
  cmd.AddValue ("routing", "Routing protocol: DSDV|AODV|OLSR|DSR", routingProtocolStr);
  cmd.AddValue ("traceMobility", "Enable mobility tracing", enableMobilityTrace);
  cmd.AddValue ("enableFlowMonitor", "Enable FlowMonitor", enableFlowMonitor);
  cmd.AddValue ("csvFileName", "CSV output file", csvFileName);
  cmd.Parse (argc, argv);

  if (routingProtocolStr == "DSDV") routingProtocol = DSDV;
  else if (routingProtocolStr == "AODV") routingProtocol = AODV;
  else if (routingProtocolStr == "OLSR") routingProtocol = OLSR;
  else if (routingProtocolStr == "DSR") routingProtocol = DSR;
  else
    {
      std::cerr << "Unknown protocol: " << routingProtocolStr << std::endl;
      return 1;
    }

  SeedManager::SetSeed (12345);

  NodeContainer nodes;
  nodes.Create (numNodes);

  // Mobility
  MobilityHelper mobility;
  mobility.SetMobilityModel ("ns3::RandomWaypointMobilityModel",
                             "Speed", StringValue ("ns3::ConstantRandomVariable[Constant=" + std::to_string (nodeSpeed) + "]"),
                             "Pause", StringValue ("ns3::ConstantRandomVariable[Constant=0.0]"),
                             "PositionAllocator",
                             StringValue ("ns3::RandomRectanglePositionAllocator[X=ns3::UniformRandomVariable[Min=0.0|Max=" + std::to_string (areaWidth) + "],Y=ns3::UniformRandomVariable[Min=0.0|Max=" + std::to_string (areaHeight) + "]]"));
  mobility.SetPositionAllocator ("ns3::RandomRectanglePositionAllocator",
                                "X", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=" + std::to_string (areaWidth) + "]"),
                                "Y", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=" + std::to_string (areaHeight) + "]"));

  mobility.Install (nodes);

  if (enableMobilityTrace)
    {
      mobility.EnableAsciiAll ("mobility-trace.tr");
    }

  // WiFi
  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211b);
  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  wifiPhy.Set ("TxPowerStart", DoubleValue (txPowerDbm));
  wifiPhy.Set ("TxPowerEnd", DoubleValue (txPowerDbm));
  wifiPhy.Set ("RxGain", DoubleValue (0));
  wifiPhy.Set ("TxGain", DoubleValue (0));
  wifiPhy.Set ("EnergyDetectionThreshold", DoubleValue (-90.0));
  wifiPhy.Set ("CcaMode1Threshold", DoubleValue (-90.0));
  wifiPhy.SetPcapDataLinkType (YansWifiPhyHelper::DLT_IEEE802_11_RADIO);

  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
  wifiPhy.SetChannel (wifiChannel.Create ());
  WifiMacHelper wifiMac;
  wifiMac.SetType ("ns3::AdhocWifiMac");

  NetDeviceContainer devices = wifi.Install (wifiPhy, wifiMac, nodes);

  // Internet stack and MANET routing protocol
  InternetStackHelper internetStack;
  OlsrHelper olsr;
  AodvHelper aodv;
  DsdvHelper dsdv;
  DsrHelper dsr;
  DsrMainHelper dsrMain;

  Ipv4ListRoutingHelper list;
  switch (routingProtocol)
    {
    case DSDV:
      list.Add (dsdv, 10);
      internetStack.SetRoutingHelper (list);
      internetStack.Install (nodes);
      break;
    case AODV:
      list.Add (aodv, 10);
      internetStack.SetRoutingHelper (list);
      internetStack.Install (nodes);
      break;
    case OLSR:
      list.Add (olsr, 10);
      internetStack.SetRoutingHelper (list);
      internetStack.Install (nodes);
      break;
    case DSR:
      internetStack.Install (nodes);
      break;
    }

  // Address
  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.0.0.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = ipv4.Assign (devices);

  if (routingProtocol == DSR)
    {
      dsrMain.Install (dsr, nodes);
    }

  // UDP Applications
  uint16_t port = 5000;
  ApplicationContainer apps;
  Ptr<UniformRandomVariable> randomVar = CreateObject<UniformRandomVariable> ();
  std::vector<std::pair<uint32_t,uint32_t>> flowPairs;
  std::set<uint32_t> usedSources;
  std::set<uint32_t> usedDests;

  // Unique sources and destinations
  while (flowPairs.size () < numFlows)
    {
      uint32_t src = randomVar->GetInteger (0, numNodes-1);
      uint32_t dst = randomVar->GetInteger (0, numNodes-1);
      if (src == dst) continue;
      bool duplicate = false;
      for (const auto& p : flowPairs)
        {
          if (p.first == src && p.second == dst)
            {
              duplicate = true;
              break;
            }
        }
      if (!duplicate)
        {
          flowPairs.push_back (std::make_pair (src, dst));
        }
    }

  for (uint32_t i = 0; i < numFlows; ++i)
    {
      uint32_t src = flowPairs[i].first;
      uint32_t dst = flowPairs[i].second;
      double startTime = 50.0 + randomVar->GetValue (0, 1);

      // Sink
      UdpServerHelper server (port + i);
      ApplicationContainer serverApp = server.Install (nodes.Get (dst));
      serverApp.Start (Seconds (0.0));
      serverApp.Stop (Seconds (simTime));

      // Source
      UdpClientHelper client (interfaces.GetAddress (dst), port + i);
      client.SetAttribute ("MaxPackets", UintegerValue (4294967295u));
      client.SetAttribute ("Interval", TimeValue (Seconds (0.05)));
      client.SetAttribute ("PacketSize", UintegerValue (512));
      ApplicationContainer clientApp = client.Install (nodes.Get (src));
      clientApp.Start (Seconds (startTime));
      clientApp.Stop (Seconds (simTime));
      apps.Add (serverApp);
      apps.Add (clientApp);
    }

  FlowMonitorHelper flowmonHelper;
  Ptr<FlowMonitor> flowmon = nullptr;
  if (enableFlowMonitor)
    {
      flowmon = flowmonHelper.InstallAll ();
    }

  Simulator::Stop (Seconds (simTime));
  Simulator::Run ();

  // Gather and output results
  std::ofstream csv (csvFileName, std::ios::out | std::ios::trunc);
  csv << "Protocol,Flows,Nodes,Area,Speed,TxPower,FlowId,SrcNode,SrcAddr,DstNode,DstAddr,BytesReceived,PacketsReceived,Duration,Throughput_bps,StartTime,EndTime\n";
  for (uint32_t i = 0; i < numFlows; ++i)
    {
      uint32_t src = flowPairs[i].first;
      uint32_t dst = flowPairs[i].second;

      Ptr<UdpServer> udpServer = DynamicCast<UdpServer> (nodes.Get(dst)->GetApplication(0));
      if (udpServer == nullptr)
        {
          // Server is not first app, search in apps
          for (uint32_t j = 0; j < nodes.Get (dst)->GetNApplications (); ++j)
            {
              udpServer = DynamicCast<UdpServer> (nodes.Get(dst)->GetApplication(j));
              if (udpServer) break;
            }
        }

      uint64_t totalBytesThrough = 0;
      uint32_t totalPacketsReceived = 0;
      double duration = simTime - 50.0;
      double throughput = 0;
      if (udpServer)
        {
          totalPacketsReceived = udpServer->GetReceived ();
          totalBytesThrough = totalPacketsReceived * 512;
          throughput = (totalBytesThrough * 8.0) / duration;
        }

      Ipv4Address srcAddr = interfaces.GetAddress (src);
      Ipv4Address dstAddr = interfaces.GetAddress (dst);

      csv << RoutingProtocolTypeToString (routingProtocol) << ",";
      csv << numFlows << ",";
      csv << numNodes << ",";
      csv << "\"" << int(areaWidth) << "x" << int(areaHeight) << "\",";
      csv << nodeSpeed << ",";
      csv << txPowerDbm << ",";
      csv << i << ",";
      csv << src << ",";
      csv << srcAddr << ",";
      csv << dst << ",";
      csv << dstAddr << ",";
      csv << totalBytesThrough << ",";
      csv << totalPacketsReceived << ",";
      csv << std::fixed << std::setprecision(2) << duration << ",";
      csv << std::fixed << std::setprecision(2) << throughput << ",";
      csv << "50.0," << simTime << "\n";
    }

  csv.close ();

  if (enableFlowMonitor && flowmon)
    {
      flowmon->SerializeToXmlFile ("manet-flowmon.xml", true, true);
    }

  Simulator::Destroy ();

  return 0;
}