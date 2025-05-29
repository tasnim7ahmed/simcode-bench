#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include <fstream>
#include <string>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("ThreeNodeBottleneck");

void
TraceGoodput(std::string filename, Ptr<Application> app, Time interval)
{
  static std::ofstream goodputFile;
  static bool first = true;
  if (first)
    {
      goodputFile.open(filename);
      goodputFile << "Time(s)\tGoodput(bps)" << std::endl;
      first = false;
    }
  static uint64_t lastTotalRx = 0;
  static Time lastTime = Seconds(0.0);

  Ptr<PacketSink> sink = DynamicCast<PacketSink> (app);
  uint64_t totalRx = sink->GetTotalRx();
  Time now = Simulator::Now();
  double goodput = (totalRx - lastTotalRx) * 8.0 / (now.GetSeconds() - lastTime.GetSeconds());
  goodputFile << now.GetSeconds() << "\t" << goodput << std::endl;
  lastTotalRx = totalRx;
  lastTime = now;
  Simulator::Schedule(interval, &TraceGoodput, filename, app, interval);
}

void
TraceBytesInQueue(std::string filename, Ptr<QueueDisc> queue)
{
  static std::ofstream byteFile;
  static bool first = true;
  if (first)
    {
      byteFile.open(filename);
      byteFile << "Time(s)\tBytesInQueue" << std::endl;
      first = false;
    }
  byteFile << Simulator::Now ().GetSeconds () << "\t" << queue->GetNBytes () << std::endl;
  Simulator::Schedule (MilliSeconds (1), &TraceBytesInQueue, filename, queue);
}

void
TraceQueueLimit(std::string filename, Ptr<QueueDisc> queue)
{
  static std::ofstream limitFile;
  static bool first = true;
  if (first)
    {
      limitFile.open(filename);
      limitFile << "Time(s)\tLimitBytes" << std::endl;
      first = false;
    }
  limitFile << Simulator::Now ().GetSeconds () << "\t" << queue->GetLimit () << std::endl;
  Simulator::Schedule (MilliSeconds (10), &TraceQueueLimit, filename, queue);
}

int main (int argc, char *argv[])
{
  std::string bottleneckBandwidth = "10Mbps";
  std::string bottleneckDelay = "10ms";
  std::string queueDiscType = "PfifoFast";
  bool enableBql = false;

  CommandLine cmd;
  cmd.AddValue ("bottleneckBandwidth", "Bandwidth of bottleneck (n2-n3) link", bottleneckBandwidth);
  cmd.AddValue ("bottleneckDelay", "Delay of bottleneck (n2-n3) link", bottleneckDelay);
  cmd.AddValue ("queueDiscType", "QueueDisc type: PfifoFast, ARED, CoDel, FqCoDel, PIE", queueDiscType);
  cmd.AddValue ("enableBql", "Enable Byte Queue Limits (BQL) on bottleneck device", enableBql);
  cmd.Parse (argc, argv);

  NodeContainer nodes;
  nodes.Create (3);

  // n1 <--> n2 (access link)
  NodeContainer n1n2 = NodeContainer (nodes.Get (0), nodes.Get (1));
  // n2 <--> n3 (bottleneck)
  NodeContainer n2n3 = NodeContainer (nodes.Get (1), nodes.Get (2));

  PointToPointHelper p2pAccess;
  p2pAccess.SetDeviceAttribute ("DataRate", StringValue ("100Mbps"));
  p2pAccess.SetChannelAttribute ("Delay", StringValue ("0.1ms"));
  NetDeviceContainer devN1N2 = p2pAccess.Install (n1n2);

  PointToPointHelper p2pBottle;
  p2pBottle.SetDeviceAttribute ("DataRate", StringValue (bottleneckBandwidth));
  p2pBottle.SetChannelAttribute ("Delay", StringValue (bottleneckDelay));
  NetDeviceContainer devN2N3 = p2pBottle.Install (n2n3);

  // Optionally enable BQL
  if (enableBql)
    {
      devN2N3.Get (0)->SetAttribute ("TxQueue", StringValue ("ns3::BqlByteQueue"));
      devN2N3.Get (1)->SetAttribute ("TxQueue", StringValue ("ns3::BqlByteQueue"));
    }

  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper addr;
  addr.SetBase ("10.0.1.0", "255.255.255.0");
  Ipv4InterfaceContainer ifN1N2 = addr.Assign (devN1N2);
  addr.SetBase ("10.0.2.0", "255.255.255.0");
  Ipv4InterfaceContainer ifN2N3 = addr.Assign (devN2N3);

  // Traffic Control on bottleneck link (n2->n3)
  TrafficControlHelper tch;
  uint16_t handle = tch.SetRootQueueDisc (queueDiscType == "PfifoFast" ? "ns3::PfifoFastQueueDisc" :
                                         queueDiscType == "ARED"       ? "ns3::AREDQueueDisc" :
                                         queueDiscType == "CoDel"      ? "ns3::CoDelQueueDisc" :
                                         queueDiscType == "FqCoDel"    ? "ns3::FqCoDelQueueDisc" :
                                         queueDiscType == "PIE"        ? "ns3::PieQueueDisc" : "ns3::PfifoFastQueueDisc");
  QueueDiscContainer queueDiscs = tch.Install (devN2N3);

  // TCP flows: n1->n3 and n3->n1
  uint16_t port1 = 50000;
  uint16_t port2 = 50001;
  Address sinkLocal1 (InetSocketAddress (Ipv4Address::GetAny (), port1));
  Address sinkLocal2 (InetSocketAddress (Ipv4Address::GetAny (), port2));

  PacketSinkHelper sinkHelper1 ("ns3::TcpSocketFactory", sinkLocal1);
  ApplicationContainer sinkApps1 = sinkHelper1.Install (nodes.Get(2));

  PacketSinkHelper sinkHelper2 ("ns3::TcpSocketFactory", sinkLocal2);
  ApplicationContainer sinkApps2 = sinkHelper2.Install (nodes.Get(0));

  sinkApps1.Start (Seconds (0.0));
  sinkApps1.Stop  (Seconds (20.0));

  sinkApps2.Start (Seconds (0.0));
  sinkApps2.Stop  (Seconds (20.0));

  // BulkSend n1 -> n3
  BulkSendHelper bulkSend1 ("ns3::TcpSocketFactory", InetSocketAddress (ifN2N3.GetAddress (1), port1));
  bulkSend1.SetAttribute ("MaxBytes", UintegerValue (0));
  ApplicationContainer senderApps1 = bulkSend1.Install (nodes.Get (0));
  senderApps1.Start (Seconds (1.0));
  senderApps1.Stop  (Seconds (19.0));

  // BulkSend n3 -> n1
  BulkSendHelper bulkSend2 ("ns3::TcpSocketFactory", InetSocketAddress (ifN1N2.GetAddress (0), port2));
  bulkSend2.SetAttribute ("MaxBytes", UintegerValue (0));
  ApplicationContainer senderApps2 = bulkSend2.Install (nodes.Get (2));
  senderApps2.Start (Seconds (1.5));
  senderApps2.Stop  (Seconds (19.0));

  // ICMP Ping n1->n3
  V4PingHelper ping (ifN2N3.GetAddress (1));
  ping.SetAttribute ("Verbose", BooleanValue (false));
  ping.SetAttribute ("Interval", TimeValue (Seconds (0.5)));
  ping.SetAttribute ("Size", UintegerValue (64));
  ping.SetAttribute ("Count", UintegerValue (20));
  ApplicationContainer pingApp = ping.Install (nodes.Get (0));
  pingApp.Start (Seconds (2.0));
  pingApp.Stop (Seconds (10.0));

  // Enable flow monitor
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  // Trace queue stats
  Ptr<QueueDisc> bottleneckQueue = queueDiscs.Get (0);
  Simulator::ScheduleNow (&TraceBytesInQueue, "bottleneck-queue-bytes.txt", bottleneckQueue);
  Simulator::ScheduleNow (&TraceQueueLimit,    "bottleneck-queue-limit.txt", bottleneckQueue);

  // Trace goodput for n1->n3 and n3->n1
  Simulator::Schedule (Seconds (1.0), &TraceGoodput, "n1-to-n3-goodput.txt", sinkApps1.Get (0), Seconds (0.5));
  Simulator::Schedule (Seconds (1.0), &TraceGoodput, "n3-to-n1-goodput.txt", sinkApps2.Get (0), Seconds (0.5));

  Simulator::Stop (Seconds (20.0));
  Simulator::Run ();

  // Output flow monitor summaries
  monitor->CheckForLostPackets ();
  std::ofstream flowf ("flow-monitor-stats.txt");
  monitor->SerializeToXmlFile ("flow-monitor.xml", true, true);

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();
  for (auto i = stats.begin(); i != stats.end(); ++i)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
      flowf << "Flow " << i->first << " (" << t.sourceAddress << "->" << t.destinationAddress << ")\n";
      flowf << "  Tx Bytes:   " << i->second.txBytes << "\n";
      flowf << "  Rx Bytes:   " << i->second.rxBytes << "\n";
      flowf << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds () - i->second.timeFirstTxPacket.GetSeconds ()) << " bps\n";
      flowf << "  Lost Packets: " << i->second.lostPackets << "\n";
      flowf << "  Mean Delay:   " << (i->second.delaySum.GetSeconds () / i->second.rxPackets) << " s\n";
    }
  flowf.close ();

  Simulator::Destroy ();
  return 0;
}