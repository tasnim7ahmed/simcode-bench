#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("ThreeNodeSimulation");

int main (int argc, char *argv[])
{
  bool enableBql = true;
  std::string queueDiscType = "PfifoFast";
  std::string bottleneckBandwidth = "1Mbps";
  std::string bottleneckDelay = "1ms";

  CommandLine cmd;
  cmd.AddValue ("enableBql", "Enable Byte Queue Limits (BQL)", enableBql);
  cmd.AddValue ("queueDiscType", "Queue Disc Type: PfifoFast, ARED, CoDel, FqCoDel, PIE", queueDiscType);
  cmd.AddValue ("bottleneckBandwidth", "Bottleneck bandwidth", bottleneckBandwidth);
  cmd.AddValue ("bottleneckDelay", "Bottleneck delay", bottleneckDelay);
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);

  NodeContainer nodes;
  nodes.Create (3);

  PointToPointHelper p2pAccess;
  p2pAccess.SetDeviceAttribute ("DataRate", StringValue ("100Mbps"));
  p2pAccess.SetChannelAttribute ("Delay", StringValue ("0.1ms"));

  NetDeviceContainer devices12 = p2pAccess.Install (nodes.Get (0), nodes.Get (1));

  PointToPointHelper p2pBottleneck;
  p2pBottleneck.SetDeviceAttribute ("DataRate", StringValue (bottleneckBandwidth));
  p2pBottleneck.SetChannelAttribute ("Delay", StringValue (bottleneckDelay));

  NetDeviceContainer devices23 = p2pBottleneck.Install (nodes.Get (1), nodes.Get (2));

  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces12 = address.Assign (devices12);

  address.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces23 = address.Assign (devices23);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  BulkSendHelper source ("ns3::TcpSocketFactory", InetSocketAddress (interfaces23.GetAddress (1), 50000));
  source.SetAttribute ("SendSize", UintegerValue (1448));
  ApplicationContainer sourceApps = source.Install (nodes.Get (0));
  sourceApps.Start (Seconds (1.0));
  sourceApps.Stop (Seconds (10.0));

  PacketSinkHelper sink ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), 50000));
  ApplicationContainer sinkApps = sink.Install (nodes.Get (2));
  sinkApps.Start (Seconds (0.0));
  sinkApps.Stop (Seconds (10.0));

  V4PingHelper ping(interfaces23.GetAddress (1));
  ping.SetAttribute ("Verbose", BooleanValue (true));
  ping.SetAttribute ("Interval", TimeValue (Seconds (0.1)));
  ApplicationContainer pingApps = ping.Install (nodes.Get(0));
  pingApps.Start (Seconds (2.0));
  pingApps.Stop (Seconds (10.0));

  TrafficControlHelper tch;
  QueueDiscContainer qdiscs;

  if (queueDiscType == "PfifoFast") {
        tch.SetRootQueueDisc ("ns3::PfifoFastQueueDisc");
  } else if (queueDiscType == "ARED") {
        tch.SetRootQueueDisc ("ns3::RedQueueDisc");
  } else if (queueDiscType == "CoDel") {
        tch.SetRootQueueDisc ("ns3::CoDelQueueDisc");
  } else if (queueDiscType == "FqCoDel") {
        tch.SetRootQueueDisc ("ns3::FqCoDelQueueDisc");
  } else if (queueDiscType == "PIE") {
        tch.SetRootQueueDisc ("ns3::PieQueueDisc");
  }

  if (enableBql)
    {
      tch.SetQueueLimits ("ns3::DynamicQueueLimits");
    }
  else
    {
      tch.SetQueueLimits ("ns3::DropTailQueue", "MaxPackets", UintegerValue (100));
    }

  qdiscs = tch.Install (devices23.Get (1));

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  Simulator::Stop (Seconds (10.0));

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
      std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1000000 << " Mbps\n";
    }

  monitor->SerializeToXmlFile("threenode.flowmon", true, true);

  Simulator::Destroy ();

  return 0;
}