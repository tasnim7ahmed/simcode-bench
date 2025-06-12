/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/**
 * Ad hoc network simulation with 10 nodes, AODV, UDP echo circular flows,
 * random mobility, PCAP tracing, routing table tracing, flow monitor and animation.
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/aodv-module.h"
#include "ns3/udp-echo-helper.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"
#include <iomanip>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("AdhocAodvEchoCircularExample");

void
PrintRoutingTables (Ptr<Node> node, Ptr<aodv::AodvRoutingProtocol> aodv, Time printTime)
{
  std::ostringstream os;
  aodv->PrintRoutingTable (os);
  std::cout << "Routing table of node " << node->GetId () << " at time "
            << Simulator::Now ().GetSeconds () << "s\n" << os.str () << std::endl;
}

void
ScheduleRoutingTablePrints (NodeContainer nodes, double interval, double stopTime)
{
  for (uint32_t i = 0; i < nodes.GetN (); ++i)
    {
      Ptr<Node> node = nodes.Get (i);
      Ptr<Ipv4> ipv4 = node->GetObject<Ipv4> ();
      Ptr<Ipv4RoutingProtocol> proto = ipv4->GetRoutingProtocol ();
      Ptr<aodv::AodvRoutingProtocol> aodv =
        DynamicCast<aodv::AodvRoutingProtocol> (proto);
      if (aodv)
        {
          for (double t = 0; t <= stopTime; t += interval)
            {
              Simulator::Schedule (Seconds (t),
                                   &PrintRoutingTables, node, aodv, Seconds (t));
            }
        }
    }
}

int
main (int argc, char *argv[])
{
  uint32_t numNodes = 10;
  double simTime = 50;
  double areaSize = 100.0;
  double printRtInterval = 10.0;
  bool printRtTables = true;

  CommandLine cmd;
  cmd.AddValue ("simTime", "Simulation time in seconds", simTime);
  cmd.AddValue ("areaSize", "Size of square area side (meters)", areaSize);
  cmd.AddValue ("printRtInterval", "Routing table print interval (seconds)", printRtInterval);
  cmd.AddValue ("printRtTables", "Enable routing table printouts", printRtTables);
  cmd.Parse (argc, argv);

  // 1. Create nodes
  NodeContainer nodes;
  nodes.Create (numNodes);

  // 2. Configure wireless PHY/MAC helpers for ad hoc Wi-Fi
  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211b);

  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetPcapDataLinkType (YansWifiPhyHelper::DLT_IEEE802_11_RADIO);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  phy.SetChannel (channel.Create ());

  WifiMacHelper mac;
  mac.SetType ("ns3::AdhocWifiMac");

  NetDeviceContainer devices = wifi.Install (phy, mac, nodes);

  // 3. Set mobility: grid layout, random walk
  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                "MinX", DoubleValue (20.0),
                                "MinY", DoubleValue (20.0),
                                "DeltaX", DoubleValue (25.0),
                                "DeltaY", DoubleValue (25.0),
                                "GridWidth", UintegerValue (5),
                                "LayoutType", StringValue ("RowFirst"));

  mobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                            "Mode", StringValue ("Time"),
                            "Time", TimeValue (Seconds (2.0)),
                            "Speed", StringValue ("ns3::UniformRandomVariable[Min=2.0|Max=4.0]"),
                            "Bounds", RectangleValue (Rectangle (0.0, areaSize, 0.0, areaSize)));
  mobility.Install (nodes);

  // 4. Install Internet + AODV
  InternetStackHelper stack;
  AodvHelper aodv;
  stack.SetRoutingHelper (aodv);
  stack.Install (nodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.0.0", "255.255.255.0");
  Ipv4InterfaceContainer ifaces = ipv4.Assign (devices);

  // 5. UDP echo server/client apps, circular topology
  uint16_t echoPort = 9;
  double start = 1.0;
  double stop = simTime;

  for (uint32_t i = 0; i < numNodes; ++i)
    {
      // Install UDP echo server on each node
      UdpEchoServerHelper echoServer (echoPort);
      ApplicationContainer serverApps = echoServer.Install (nodes.Get (i));
      serverApps.Start (Seconds (start));
      serverApps.Stop (Seconds (stop));
    }

  for (uint32_t i = 0; i < numNodes; ++i)
    {
      uint32_t target = (i + 1) % numNodes;
      UdpEchoClientHelper echoClient (ifaces.GetAddress (target), echoPort);
      echoClient.SetAttribute ("MaxPackets", UintegerValue (uint32_t ((simTime - start) / 4.0)));
      echoClient.SetAttribute ("Interval", TimeValue (Seconds (4.0)));
      echoClient.SetAttribute ("PacketSize", UintegerValue (64));

      ApplicationContainer clientApps = echoClient.Install (nodes.Get (i));
      clientApps.Start (Seconds (start + 0.5));
      clientApps.Stop (Seconds (stop));
    }

  // 6. Enable PCAP tracing
  phy.EnablePcap ("adhoc-aodv-grid", devices, true);

  // 7. (Optional) Routing table printouts
  if (printRtTables)
    {
      ScheduleRoutingTablePrints (nodes, printRtInterval, simTime - 0.1);
    }

  // 8. Flow Monitor for stats
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  // 9. NetAnim animation
  AnimationInterface anim ("adhoc-aodv-grid.xml");
  anim.SetMaxPktsPerTraceFile (500000);

  // 10. Run simulation
  Simulator::Stop (Seconds (simTime));
  Simulator::Run ();

  // 11. Output FlowMonitor statistics
  monitor->CheckForLostPackets ();
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats ();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());

  uint32_t sentPackets = 0, recvPackets = 0;
  double totalDelay = 0.0;
  double totalThroughput = 0.0;

  for (auto const &fp : stats)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (fp.first);

      sentPackets += fp.second.txPackets;
      recvPackets += fp.second.rxPackets;
      totalDelay += fp.second.delaySum.GetSeconds ();
      if (fp.second.rxPackets > 0)
        {
          double throughput =
            (fp.second.rxBytes * 8.0) / (fp.second.timeLastRxPacket.GetSeconds () -
                                         fp.second.timeFirstTxPacket.GetSeconds ()) / 1000.0; // kbps
          totalThroughput += throughput;
        }
    }
  double pdr = (sentPackets > 0) ? 100.0 * (double)recvPackets / (double)sentPackets : 0.0;
  double avgDelay = (recvPackets > 0) ? totalDelay / recvPackets : 0.0;
  double avgThroughput = (stats.size () > 0) ? totalThroughput / stats.size () : 0.0;

  std::cout << std::fixed << std::setprecision (2);
  std::cout << "\n===== Simulation Results =====\n";
  std::cout << "  Sent Packets:     " << sentPackets << std::endl;
  std::cout << "  Received Packets: " << recvPackets << std::endl;
  std::cout << "  PDR (%):          " << pdr << std::endl;
  std::cout << "  Avg Delay (s):    " << avgDelay << std::endl;
  std::cout << "  Avg Throughput (kbps): " << avgThroughput << std::endl;

  Simulator::Destroy ();
  return 0;
}