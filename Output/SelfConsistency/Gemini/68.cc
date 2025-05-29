#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/internet-module.h"
#include "ns3/ping-helper.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("QueueDiscComparison");

int main (int argc, char *argv[])
{
  bool enableBql = false;
  std::string queueDiscType = "PfifoFast";
  std::string bottleneckBandwidth = "10Mbps";
  std::string bottleneckDelay = "1ms";
  uint32_t maxBytes = 65535;

  CommandLine cmd;
  cmd.AddValue ("enableBql", "Enable Byte Queue Limits (BQL)", enableBql);
  cmd.AddValue ("queueDiscType", "Queue disc type: PfifoFast, ARED, CoDel, FqCoDel, PIE", queueDiscType);
  cmd.AddValue ("bottleneckBandwidth", "Bottleneck bandwidth", bottleneckBandwidth);
  cmd.AddValue ("bottleneckDelay", "Bottleneck delay", bottleneckDelay);
  cmd.AddValue ("maxBytes", "Max Bytes for queue", maxBytes);
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);
  LogComponent::SetDefaultLogLevel (LogLevel::LOG_PREFIX_TIME);
  LogComponent::SetPrintLevel (LogLevel::LOG_ALL);

  NodeContainer nodes;
  nodes.Create (3);

  PointToPointHelper pointToPointAccess;
  pointToPointAccess.SetDeviceAttribute ("DataRate", StringValue ("100Mbps"));
  pointToPointAccess.SetChannelAttribute ("Delay", StringValue ("0.1ms"));

  NetDeviceContainer devices12 = pointToPointAccess.Install (nodes.Get (0), nodes.Get (1));

  PointToPointHelper pointToPointBottleneck;
  pointToPointBottleneck.SetDeviceAttribute ("DataRate", StringValue (bottleneckBandwidth));
  pointToPointBottleneck.SetChannelAttribute ("Delay", StringValue (bottleneckDelay));

  NetDeviceContainer devices23 = pointToPointBottleneck.Install (nodes.Get (1), nodes.Get (2));

  InternetStackHelper stack;
  stack.InstallAll ();

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces12 = address.Assign (devices12);

  address.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces23 = address.Assign (devices23);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // Configure Queue Disc
  TrafficControlHelper tch;
  tch.SetRootQueueDisc ("ns3::" + queueDiscType);
  if (queueDiscType == "Ared") {
      tch.AddAttribute ("LinkBandwidth", StringValue (bottleneckBandwidth));
  }
  if (queueDiscType == "CoDel" || queueDiscType == "FqCoDel" || queueDiscType == "Pie") {
      tch.SetQueueLimits (maxBytes); // bytes
  }

  QueueDiscContainer queueDiscs = tch.Install (devices23.Get (1));

  // Enable BQL if requested
  if (enableBql)
    {
      // Enable BQL on the bottleneck link
      devices23.Get (1)->SetAttribute ("EnableBql", BooleanValue (true));
    }

  uint16_t port = 50000;
  BulkSendHelper source ("ns3::TcpSocketFactory",
                         InetSocketAddress (interfaces23.GetAddress (1), port));
  source.SetAttribute ("SendSize", UintegerValue (1400));

  ApplicationContainer sourceApps = source.Install (nodes.Get (0));
  sourceApps.Start (Seconds (1.0));
  sourceApps.Stop (Seconds (10.0));

  PacketSinkHelper sink ("ns3::TcpSocketFactory",
                         InetSocketAddress (Ipv4Address::GetAny (), port));
  ApplicationContainer sinkApps = sink.Install (nodes.Get (2));
  sinkApps.Start (Seconds (0.0));
  sinkApps.Stop (Seconds (10.0));

    // Configure and start a ping application from node 0 to node 2
  PingHelper pingHelper (interfaces23.GetAddress (1));
  pingHelper.SetAttribute ("Interval", TimeValue (Seconds (0.1)));
  ApplicationContainer pingApp = pingHelper.Install (nodes.Get (0));
  pingApp.Start (Seconds (2.0));
  pingApp.Stop (Seconds (10.0));

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  // Tracing
  AsciiTraceHelper ascii;
  pointToPointBottleneck.EnableAsciiAll (ascii.CreateFileStream ("queue-comparison.tr"));

  // Add queue limit trace
  if (enableBql)
  {
        Config::ConnectWithoutContext ("/NodeList/1/DeviceList/1/$ns3::PointToPointNetDevice/TxQueue/BytesInQueue",
                                MakeCallback (&QueueSizeTrace));

        Config::ConnectWithoutContext ("/NodeList/1/DeviceList/1/$ns3::PointToPointNetDevice/TxQueue/MaxBytes",
                                MakeCallback (&QueueLimitTrace));
  }

  Simulator::Stop (Seconds (10.0));

  Simulator::Run ();

  monitor->CheckForLostPackets ();

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
      std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
      std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
      std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
      std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds () - i->second.timeFirstTxPacket.GetSeconds ()) / 1000000 << " Mbps\n";
    }

  monitor->SerializeToXmlFile("queue-comparison.flowmon", true, true);

  Simulator::Destroy ();

  return 0;
}

static void
QueueSizeTrace (uint32_t oldVal, uint32_t newVal)
{
  std::cout << Simulator::Now ().GetSeconds () << " Queue Size: " << newVal << std::endl;
}

static void
QueueLimitTrace (uint32_t oldVal, uint32_t newVal)
{
  std::cout << Simulator::Now ().GetSeconds () << " Queue Limit: " << newVal << std::endl;
}