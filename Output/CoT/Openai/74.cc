#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/wifi-module.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/udp-client-server-helper.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/dsdv-helper.h"
#include "ns3/aodv-helper.h"
#include "ns3/olsr-helper.h"
#include "ns3/dsr-helper.h"
#include "ns3/dsr-main-helper.h"
#include <fstream>
#include <iomanip>
#include <sstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("ManetProtocolsComparison");

enum RoutingProtocol
{
  OLSR,
  AODV,
  DSDV,
  DSR
};

static std::string
RoutingProtocolToString (RoutingProtocol protocol)
{
  switch (protocol)
    {
    case OLSR: return "OLSR";
    case AODV: return "AODV";
    case DSDV: return "DSDV";
    case DSR:  return "DSR";
    default:   return "UNKNOWN";
    }
}

int main (int argc, char *argv[])
{
  uint32_t nodeCount = 50;
  double simTime = 200.0;
  uint32_t udpPairs = 10;
  double areaX = 1500.0;
  double areaY = 300.0;
  double nodeSpeed = 20.0;
  double txPower = 7.5;
  RoutingProtocol routingProtocol = OLSR;
  bool enableMobilityTracing = false;
  bool enableFlowMonitor = false;
  std::string csvFileName = "manet_results.csv";

  CommandLine cmd;
  cmd.AddValue ("nodes", "Number of nodes", nodeCount);
  cmd.AddValue ("simTime", "Simulation time in seconds", simTime);
  cmd.AddValue ("udpPairs", "Number of CBR UDP flows", udpPairs);
  cmd.AddValue ("areaX", "Simulation area length (x, meters)", areaX);
  cmd.AddValue ("areaY", "Simulation area width (y, meters)", areaY);
  cmd.AddValue ("speed", "Node speed (m/s)", nodeSpeed);
  cmd.AddValue ("txPower", "Transmission power in dBm", txPower);
  cmd.AddValue ("protocol", "Routing protocol: OLSR, AODV, DSDV, or DSR", routingProtocol);
  cmd.AddValue ("csvFile", "CSV output file for results", csvFileName);
  cmd.AddValue ("mobilityTrace", "Enable mobility tracing", enableMobilityTracing);
  cmd.AddValue ("flowMonitor", "Enable flow monitoring", enableFlowMonitor);

  std::string protoStr;
  cmd.AddValue ("protocolStr", "Routing protocol as string: OLSR, AODV, DSDV, or DSR", protoStr);

  cmd.Parse (argc, argv);

  if (!protoStr.empty())
    {
      if (protoStr == "OLSR") routingProtocol = OLSR;
      else if (protoStr == "AODV") routingProtocol = AODV;
      else if (protoStr == "DSDV") routingProtocol = DSDV;
      else if (protoStr == "DSR")  routingProtocol = DSR;
    }

  SeedManager::SetSeed (1234);

  NodeContainer nodes;
  nodes.Create (nodeCount);

  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211b);

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
  wifiPhy.SetChannel (wifiChannel.Create ());
  wifiPhy.SetPcapDataLinkType (YansWifiPhyHelper::DLT_IEEE802_11_RADIO);

  wifiPhy.Set ("TxPowerStart", DoubleValue (txPower));
  wifiPhy.Set ("TxPowerEnd", DoubleValue (txPower));

  WifiMacHelper wifiMac;
  wifiMac.SetType ("ns3::AdhocWifiMac");

  NetDeviceContainer devices = wifi.Install (wifiPhy, wifiMac, nodes);

  MobilityHelper mobility;
  mobility.SetMobilityModel ("ns3::RandomWaypointMobilityModel",
                             "Speed", StringValue ("ns3::ConstantRandomVariable[Constant=" + std::to_string(nodeSpeed) + "]"),
                             "Pause", StringValue ("ns3::ConstantRandomVariable[Constant=0.0]"),
                             "PositionAllocator", PointerValue (
                               CreateObjectWithAttributes<RandomRectanglePositionAllocator>
                                 ("X", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=" + std::to_string(areaX) + "]"),
                                  "Y", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=" + std::to_string(areaY) + "]"))));
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (5.0),
                                 "DeltaY", DoubleValue (5.0),
                                 "GridWidth", UintegerValue (10),
                                 "LayoutType", StringValue ("RowFirst"));
  mobility.Install (nodes);

  if (enableMobilityTracing)
    {
      mobility.EnableAsciiAll (Create<OutputStreamWrapper> ("mobility-trace.txt", std::ios::out));
    }

  InternetStackHelper stack;

  if (routingProtocol == OLSR)
    {
      OlsrHelper olsr;
      stack.SetRoutingHelper (olsr);
      stack.Install (nodes);
    }
  else if (routingProtocol == AODV)
    {
      AodvHelper aodv;
      stack.SetRoutingHelper (aodv);
      stack.Install (nodes);
    }
  else if (routingProtocol == DSDV)
    {
      DsdvHelper dsdv;
      stack.SetRoutingHelper (dsdv);
      stack.Install (nodes);
    }
  else if (routingProtocol == DSR)
    {
      InternetStackHelper dsrStack;
      dsrStack.Install (nodes);
      DsrMainHelper dsrMain;
      DsrHelper dsr;
      dsrMain.Install (dsr, nodes);
    }

  Ipv4AddressHelper address;
  address.SetBase ("10.0.0.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  ApplicationContainer serverApps, clientApps;

  Ptr<UniformRandomVariable> randomStart = CreateObject<UniformRandomVariable> ();
  randomStart->SetAttribute ("Min", DoubleValue (50.0));
  randomStart->SetAttribute ("Max", DoubleValue (51.0));

  for (uint32_t i = 0; i < udpPairs; ++i)
    {
      uint32_t srcIdx = i % nodeCount;
      uint32_t dstIdx = (i + udpPairs) % nodeCount;
      if (srcIdx == dstIdx) dstIdx = (dstIdx + 1) % nodeCount;

      UdpServerHelper server (4000 + i);
      ApplicationContainer serverApp = server.Install (nodes.Get (dstIdx));
      serverApp.Start (Seconds (0.0));
      serverApp.Stop (Seconds (simTime));
      serverApps.Add (serverApp);

      UdpClientHelper client (interfaces.GetAddress (dstIdx), 4000 + i);
      client.SetAttribute ("MaxPackets", UintegerValue (uint32_t(-1)));
      client.SetAttribute ("Interval", TimeValue (Seconds (0.05))); // ~20 packets/s
      client.SetAttribute ("PacketSize", UintegerValue (512));
      double startTime = randomStart->GetValue ();
      ApplicationContainer clientApp = client.Install (nodes.Get (srcIdx));
      clientApp.Start (Seconds (startTime));
      clientApp.Stop (Seconds (simTime));
      clientApps.Add (clientApp);
    }

  FlowMonitorHelper flowmonitor;
  Ptr<FlowMonitor> monitor;

  if (enableFlowMonitor)
    {
      monitor = flowmonitor.InstallAll ();
    }

  Simulator::Stop (Seconds (simTime));
  Simulator::Run ();

  // Collect results
  std::ofstream out (csvFileName, std::ios::app);
  if (out.tellp() == 0)
    {
      out << "Protocol,Nodes,UDP_Flows,AreaX,AreaY,Speed,TxPower,Flow,TxPackets,TxBytes,RxPackets,RxBytes,LostPackets,ThroughputKbps,DelayMs\n";
    }

  uint32_t flowId = 0;
  for (uint32_t i = 0; i < udpPairs; ++i)
    {
      Ptr<UdpServer> server = DynamicCast<UdpServer> (serverApps.Get (i));
      uint32_t rxPackets = server ? server->GetReceived () : 0;
      uint64_t rxBytes = server ? server->GetReceivedBytes () : 0;
      uint32_t lost = server ? server->GetLost () : 0;
      double duration = simTime - 50.0;
      double throughput = (rxBytes * 8.0) / (duration * 1000.0); // kbps
      double delayMs = 0.0;
      uint32_t txPackets = 0;
      uint32_t txBytes = 0;

      if (enableFlowMonitor && monitor)
        {
          monitor->CheckForLostPackets ();
          FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats ();
          for (auto it = stats.begin (); it != stats.end (); ++it)
            {
              if (flowId == i)
                {
                  txPackets = it->second.txPackets;
                  txBytes = it->second.txBytes;
                  if (it->second.rxPackets > 0)
                    delayMs = (it->second.delaySum.GetSeconds () / it->second.rxPackets) * 1000.0;
                  break;
                }
              ++flowId;
            }
        }
      else
        {
          txPackets = 0;
          txBytes = 0;
        }

      out << RoutingProtocolToString (routingProtocol) << ","
          << nodeCount << ","
          << udpPairs << ","
          << areaX << ","
          << areaY << ","
          << nodeSpeed << ","
          << txPower << ","
          << i << ","
          << txPackets << ","
          << txBytes << ","
          << rxPackets << ","
          << rxBytes << ","
          << lost << ","
          << std::fixed << std::setprecision (2) << throughput << ","
          << std::fixed << std::setprecision (2) << delayMs << "\n";
    }

  out.close ();
  Simulator::Destroy ();
  return 0;
}