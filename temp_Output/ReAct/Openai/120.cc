#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/aodv-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("AdhocAodvGrid");

void PrintRoutingTables (Ptr<Node> node, Ptr<Ipv4RoutingHelper> routingHelper, double time) {
  Ipv4StaticRoutingHelper ipv4RoutingHelper;
  Ptr<Ipv4> ipv4 = node->GetObject<Ipv4> ();
  NS_LOG_UNCOND ("Routing table for node " << node->GetId () << " at time " << Simulator::Now ().GetSeconds () << "s");
  routingHelper->PrintRoutingTable (node, std::cout);
}

int main (int argc, char *argv[])
{
  uint32_t numNodes = 10;
  double simTime = 60.0;
  double areaWidth = 100.0;
  double areaHeight = 100.0;
  double gridSpacing = 25.0;
  double nodeSpeed = 5.0;
  double nodePause = 2.0;
  bool printRoutingTables = true;
  double routingTablePrintInterval = 10.0;

  CommandLine cmd;
  cmd.AddValue ("printRoutingTables", "Print routing tables periodically", printRoutingTables);
  cmd.AddValue ("routingTablePrintInterval", "Interval (s) between routing table prints", routingTablePrintInterval);
  cmd.Parse (argc, argv);

  NodeContainer nodes;
  nodes.Create (numNodes);

  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  wifiPhy.SetChannel (wifiChannel.Create ());

  WifiMacHelper wifiMac;
  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211b);
  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                               "DataMode", StringValue ("DsssRate11Mbps"),
                               "ControlMode", StringValue ("DsssRate11Mbps"));

  wifiMac.SetType ("ns3::AdhocWifiMac");
  NetDeviceContainer devices = wifi.Install (wifiPhy, wifiMac, nodes);

  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                "MinX", DoubleValue (0.0),
                                "MinY", DoubleValue (0.0),
                                "DeltaX", DoubleValue (gridSpacing),
                                "DeltaY", DoubleValue (gridSpacing),
                                "GridWidth", UintegerValue (4),
                                "LayoutType", StringValue ("RowFirst"));
  mobility.SetMobilityModel ("ns3::RandomWaypointMobilityModel",
                            "Speed", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=5.0]"),
                            "Pause", StringValue ("ns3::ConstantRandomVariable[Constant=2.0]"),
                            "PositionAllocator", PointerValue (mobility.GetPositionAllocator ()));
  mobility.Install (nodes);

  InternetStackHelper internet;
  AodvHelper aodv;
  internet.SetRoutingHelper (aodv);
  internet.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.0.0", "255.255.0.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  uint16_t echoPort = 9;
  ApplicationContainer serverApps, clientApps;

  for (uint32_t i = 0; i < numNodes; ++i)
    {
      UdpEchoServerHelper echoServer (echoPort);
      serverApps.Add (echoServer.Install (nodes.Get (i)));
    }
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (simTime));

  for (uint32_t i = 0; i < numNodes; ++i)
    {
      Ipv4Address dstAddress = interfaces.GetAddress ((i+1)%numNodes);
      UdpEchoClientHelper echoClient (dstAddress, echoPort);
      echoClient.SetAttribute ("MaxPackets", UintegerValue ((uint32_t)(simTime/4)-1));
      echoClient.SetAttribute ("Interval", TimeValue (Seconds (4.0)));
      echoClient.SetAttribute ("PacketSize", UintegerValue (64));
      ApplicationContainer client = echoClient.Install (nodes.Get (i));
      client.Start (Seconds (2.0));
      client.Stop (Seconds (simTime));
      clientApps.Add (client);
    }

  wifiPhy.EnablePcapAll ("adhoc-aodv-grid");

  Ptr<FlowMonitor> flowMonitor;
  FlowMonitorHelper flowHelper;
  flowMonitor = flowHelper.InstallAll ();

  AnimationInterface anim ("adhoc-aodv-grid.xml");
  anim.SetMaxPktsPerTraceFile (0);

  if (printRoutingTables)
    {
      for (double t = 5.0; t < simTime; t += routingTablePrintInterval)
        {
          for (uint32_t i = 0; i < numNodes; ++i)
            {
              Simulator::Schedule (Seconds (t), &AodvHelper::PrintRoutingTable, &aodv, nodes.Get (i), std::cout);
            }
        }
    }

  Simulator::Stop (Seconds (simTime+0.1));
  Simulator::Run ();

  flowMonitor->CheckForLostPackets ();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowHelper.GetClassifier ());
  FlowMonitor::FlowStatsContainer stats = flowMonitor->GetFlowStats ();

  uint32_t totalTxPackets = 0;
  uint32_t totalRxPackets = 0;
  double totalDelay = 0.0;
  uint32_t totalRxBytes = 0;
  double firstTxTime = -1;
  double lastRxTime = 0;

  for (auto& iter : stats)
    {
      totalTxPackets += iter.second.txPackets;
      totalRxPackets += iter.second.rxPackets;
      totalRxBytes += iter.second.rxBytes;
      totalDelay += iter.second.delaySum.GetSeconds ();
      if (iter.second.rxPackets > 0)
        {
          double flowFirstTx = iter.second.timeFirstTxPacket.GetSeconds ();
          double flowLastRx = iter.second.timeLastRxPacket.GetSeconds ();
          if (firstTxTime < 0 || flowFirstTx < firstTxTime)
            firstTxTime = flowFirstTx;
          if (flowLastRx > lastRxTime)
            lastRxTime = flowLastRx;
        }
    }

  double pdr = (totalTxPackets > 0) ? ((double)totalRxPackets/(double)totalTxPackets) : 0.0;
  double avgDelay = (totalRxPackets > 0) ? (totalDelay / (double)totalRxPackets) : 0.0;
  double throughput = (lastRxTime > firstTxTime) ? (totalRxBytes * 8.0 / (lastRxTime - firstTxTime) / 1e3) : 0.0;

  std::cout << "Simulation Results:\n";
  std::cout << "  Packet Delivery Ratio: " << pdr * 100 << "%\n";
  std::cout << "  Average End-to-End Delay: " << avgDelay * 1000 << " ms\n";
  std::cout << "  Throughput: " << throughput << " kbps\n";

  Simulator::Destroy ();
  return 0;
}