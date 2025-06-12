#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/internet-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ping-application.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("ThreeNodeQdisc");

int main (int argc, char *argv[])
{
  bool enableBql = false;
  std::string bottleneckBandwidth = "10Mbps";
  std::string bottleneckDelay = "2ms";
  std::string queueDiscType = "PfifoFast";

  CommandLine cmd;
  cmd.AddValue ("enableBql", "Enable Byte Queue Limits (BQL)", enableBql);
  cmd.AddValue ("bottleneckBandwidth", "Bottleneck bandwidth", bottleneckBandwidth);
  cmd.AddValue ("bottleneckDelay", "Bottleneck delay", bottleneckDelay);
  cmd.AddValue ("queueDiscType", "Queue disc type (PfifoFast, ARED, CoDel, FqCoDel, PIE)", queueDiscType);
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);
  LogComponent::SetDefaultLogLevel (LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create (3);

  PointToPointHelper pointToPointLeaf;
  pointToPointLeaf.SetDeviceAttribute ("DataRate", StringValue ("100Mbps"));
  pointToPointLeaf.SetChannelAttribute ("Delay", StringValue ("0.1ms"));

  NetDeviceContainer devicesLeaf0 = pointToPointLeaf.Install (nodes.Get (0), nodes.Get (1));
  NetDeviceContainer devicesLeaf1 = pointToPointLeaf.Install (nodes.Get (2), nodes.Get (1));

  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfacesLeaf0 = address.Assign (devicesLeaf0);
  address.NewNetwork ();
  Ipv4InterfaceContainer interfacesLeaf1 = address.Assign (devicesLeaf1);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  PointToPointHelper pointToPointBottleneck;
  pointToPointBottleneck.SetDeviceAttribute ("DataRate", StringValue (bottleneckBandwidth));
  pointToPointBottleneck.SetChannelAttribute ("Delay", StringValue (bottleneckDelay));

  TrafficControlHelper tchBottleneck;
  QueueDiscTypeId queueDiscTypeId;

  if (queueDiscType == "ARED")
    {
      queueDiscTypeId = TypeId::LookupByName ("ns3::RedQueueDisc");
    }
  else if (queueDiscType == "CoDel")
    {
      queueDiscTypeId = TypeId::LookupByName ("ns3::CoDelQueueDisc");
    }
  else if (queueDiscType == "FqCoDel")
    {
      queueDiscTypeId = TypeId::LookupByName ("ns3::FqCoDelQueueDisc");
    }
  else if (queueDiscType == "PIE")
    {
      queueDiscTypeId = TypeId::LookupByName ("ns3::PieQueueDisc");
    }
  else
    {
      queueDiscTypeId = TypeId::LookupByName ("ns3::PfifoFastQueueDisc");
    }

  tchBottleneck.SetRootQueueDisc ("ns3::QueueDiscWrapper", "Mode", StringValue ("QUEUE_MODE_PACKETS"), "QueueDisc", PointerValue (CreateObject (queueDiscTypeId)));

  if (enableBql)
    {
      tchBottleneck.SetQueueLimits ("ns3::DynamicQueueLimits", "HoldTime", StringValue ("1000us"));
    }

  tchBottleneck.Install (devicesLeaf1.Get (0));

  BulkSendHelper source ("ns3::TcpSocketFactory", InetSocketAddress (interfacesLeaf1.GetAddress (0), 9));
  source.SetAttribute ("SendSize", UintegerValue (1448));
  ApplicationContainer sourceApps = source.Install (nodes.Get (0));
  sourceApps.Start (Seconds (1.0));
  sourceApps.Stop (Seconds (10.0));

  PacketSinkHelper sink ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), 9));
  ApplicationContainer sinkApps = sink.Install (nodes.Get (2));
  sinkApps.Start (Seconds (0.0));
  sinkApps.Stop (Seconds (10.0));

  PingApplicationHelper pingAppHelper (interfacesLeaf1.GetAddress (0));
  pingAppHelper.SetAttribute ("Interval", TimeValue (Seconds (0.1)));
  pingAppHelper.SetAttribute ("StartTime", TimeValue (Seconds (2.0)));
  pingAppHelper.SetAttribute ("StopTime", TimeValue (Seconds (10.0)));
  ApplicationContainer pingApps = pingAppHelper.Install (nodes.Get (0));

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  Simulator::Stop (Seconds (10.0));

  AsciiTraceHelper ascii;
  pointToPointBottleneck.EnableAsciiAll (ascii.CreateFileStream ("three-node-qdisc.tr"));

  Simulator::Run ();

  monitor->CheckForLostPackets ();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats ();

  for (auto i = stats.begin (); i != stats.end (); ++i)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
      std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
      std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
      std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
      std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds () - i->second.timeFirstTxPacket.GetSeconds ()) / 1000000 << " Mbps\n";
    }

  monitor->SerializeToXmlFile("three-node-qdisc.flowmon", true, true);

  Simulator::Destroy ();
  return 0;
}