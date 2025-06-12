#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/queue-size.h"
#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("ThreeNodeBottleneckSim");

void
GoodputTracer (Ptr<OutputStreamWrapper> stream,
               std::string context,
               uint32_t oldValue,
               uint32_t newValue)
{
  static std::map<std::string, uint64_t> lastRx, lastTime;
  uint64_t now = Simulator::Now ().GetSeconds () * 1000000; // us

  if (lastTime.find (context) != lastTime.end ())
    {
      double dt = (now - lastTime[context]) / 1e6;
      double goodput = (double)(newValue - lastRx[context]) * 8 / dt / 1000000.0; // Mbps
      *stream->GetStream () << Simulator::Now ().GetSeconds () << "s\t" << context << "\t" << goodput << " Mbps\n";
    }
  lastTime[context] = now;
  lastRx[context] = newValue;
}

void
QueueSizeTracer (Ptr<QueueDisc> queue,
                 Ptr<OutputStreamWrapper> stream)
{
  uint32_t qSize = queue->GetCurrentSize ().GetValue ();
  *stream->GetStream () << Simulator::Now ().GetSeconds ()
                       << "\t" << qSize << '\n';
  Simulator::Schedule (Seconds (0.001), &QueueSizeTracer, queue, stream); // 1ms interval
}

void
QueueLimitTracer (Ptr<QueueDisc> queue,
                  Ptr<OutputStreamWrapper> stream)
{
  uint32_t limit = queue->GetQueueSize ();
  *stream->GetStream () << Simulator::Now ().GetSeconds ()
                       << "\t" << limit << '\n';
  Simulator::Schedule (Seconds (0.1), &QueueLimitTracer, queue, stream); // 100ms interval
}

int
main (int argc, char *argv[])
{
  std::string bottleneckBw = "10Mbps";
  std::string bottleneckDelay = "10ms";
  std::string queueDiscType = "PfifoFastQueueDisc";
  bool enableBql = false;
  uint32_t maxBytes = 10000;
  CommandLine cmd;
  cmd.AddValue ("bottleneckBw", "Bandwidth of the bottleneck link", bottleneckBw);
  cmd.AddValue ("bottleneckDelay", "Delay of the bottleneck link", bottleneckDelay);
  cmd.AddValue ("queueDiscType", "Queue disc type (PfifoFastQueueDisc|AREDQueueDisc|CoDelQueueDisc|FqCoDelQueueDisc|PieQueueDisc)", queueDiscType);
  cmd.AddValue ("enableBql", "Enable Byte Queue Limits (BQL)", enableBql);
  cmd.AddValue ("maxBytes", "Max bytes for the bottleneck device queue (if BQL enabled)", maxBytes);
  cmd.Parse (argc, argv);

  NodeContainer nodes;
  nodes.Create (3); // n0=n1, n1=n2, n2=n3

  // n1 <-> n2 ("access" link)
  NodeContainer n1n2 = NodeContainer (nodes.Get (0), nodes.Get (1));
  // n2 <-> n3 ("bottleneck" link)
  NodeContainer n2n3 = NodeContainer (nodes.Get (1), nodes.Get (2));

  PointToPointHelper p2pA;
  p2pA.SetDeviceAttribute ("DataRate", StringValue ("100Mbps"));
  p2pA.SetChannelAttribute ("Delay", StringValue ("0.1ms"));
  NetDeviceContainer devA = p2pA.Install (n1n2);

  PointToPointHelper p2pB;
  p2pB.SetDeviceAttribute ("DataRate", StringValue (bottleneckBw));
  p2pB.SetChannelAttribute ("Delay", StringValue (bottleneckDelay));
  if (enableBql)
    {
      p2pB.SetQueue ("ns3::DropTailQueue",
                     "MaxSize", QueueSizeValue (QueueSize (QueueSizeUnit::BYTES, maxBytes)));
    }
  NetDeviceContainer devB = p2pB.Install (n2n3);

  InternetStackHelper internet;
  internet.Install (nodes);

  Ipv4AddressHelper ipv4A, ipv4B;
  ipv4A.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer intA = ipv4A.Assign (devA);

  ipv4B.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer intB = ipv4B.Assign (devB);

  TrafficControlHelper tch;
  uint16_t handle = tch.SetRootQueueDisc (queueDiscType);
  tch.Install (devB);

  // Pin down QueueDisc pointer for tracing
  Ptr<NetDevice> bottleneckDev = devB.Get (0);
  Ptr<PointToPointNetDevice> ptpDev = DynamicCast<PointToPointNetDevice> (bottleneckDev);
  Ptr<Queue<Packet> > txQueue = ptpDev->GetQueue ();
  Ptr<QueueDisc> queueDisc = StaticCast<QueueDisc> (
    bottleneckDev->GetObject<TrafficControlLayer> ()->GetRootQueueDiscOnDevice (bottleneckDev));

  // Bidirectional TCP flows: n1->n3 and n3->n1
  uint16_t port1 = 5000, port2 = 5001;
  // n1 sends to n3
  Address sinkAddress1 (InetSocketAddress (intB.GetAddress (1), port1));
  PacketSinkHelper sinkHelper1 ("ns3::TcpSocketFactory", sinkAddress1);
  ApplicationContainer sinkApp1 = sinkHelper1.Install (nodes.Get (2));

  // n3 sends to n1
  Address sinkAddress2 (InetSocketAddress (intA.GetAddress (0), port2));
  PacketSinkHelper sinkHelper2 ("ns3::TcpSocketFactory", sinkAddress2);
  ApplicationContainer sinkApp2 = sinkHelper2.Install (nodes.Get (0));

  sinkApp1.Start (Seconds (0.0));
  sinkApp1.Stop (Seconds (20.0));
  sinkApp2.Start (Seconds (0.0));
  sinkApp2.Stop (Seconds (20.0));

  OnOffHelper sender1 ("ns3::TcpSocketFactory", sinkAddress1);
  sender1.SetAttribute ("DataRate", DataRateValue (DataRate ("100Mbps")));
  sender1.SetAttribute ("PacketSize", UintegerValue (1448));
  sender1.SetAttribute ("StartTime", TimeValue (Seconds (1.0)));
  sender1.SetAttribute ("StopTime", TimeValue (Seconds (19.0)));
  sender1.Install (nodes.Get (0));

  OnOffHelper sender2 ("ns3::TcpSocketFactory", sinkAddress2);
  sender2.SetAttribute ("DataRate", DataRateValue (DataRate ("100Mbps")));
  sender2.SetAttribute ("PacketSize", UintegerValue (1448));
  sender2.SetAttribute ("StartTime", TimeValue (Seconds (1.0)));
  sender2.SetAttribute ("StopTime", TimeValue (Seconds (19.0)));
  sender2.Install (nodes.Get (2));

  // Ping from n1 to n3
  V4PingHelper pingHelper (intB.GetAddress (1));
  pingHelper.SetAttribute ("Verbose", BooleanValue (false));
  ApplicationContainer pingApps = pingHelper.Install (nodes.Get (0));
  pingApps.Start (Seconds (2.0));
  pingApps.Stop (Seconds (18.0));

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  // Tracing
  AsciiTraceHelper ascii;
  auto queueStream = ascii.CreateFileStream ("queue-bytes.tr");
  Simulator::ScheduleNow (&QueueSizeTracer, queueDisc, queueStream);

  auto limitStream = ascii.CreateFileStream ("queue-limit.tr");
  Simulator::ScheduleNow (&QueueLimitTracer, queueDisc, limitStream);

  auto goodputStream = ascii.CreateFileStream ("goodput.tr");
  sinkApp1.Get (0)->TraceConnectWithoutContext (
      "Rx", MakeBoundCallback (&GoodputTracer, goodputStream, "n1-to-n3"));
  sinkApp2.Get (0)->TraceConnectWithoutContext (
      "Rx", MakeBoundCallback (&GoodputTracer, goodputStream, "n3-to-n1"));

  Simulator::Stop (Seconds (20.0));
  Simulator::Run ();

  // Save flow monitor statistics
  monitor->CheckForLostPackets ();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  std::ofstream flowStatsFile ("flowmon-results.txt");

  auto stats = monitor->GetFlowStats ();
  for (const auto &flow : stats)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (flow.first);
      flowStatsFile << "FlowID=" << flow.first << " "
                    << t.sourceAddress << "->" << t.destinationAddress
                    << " (" << (int)t.protocol << ")\n";
      flowStatsFile << "  Tx Bytes: " << flow.second.txBytes << "\n";
      flowStatsFile << "  Rx Bytes: " << flow.second.rxBytes << "\n";
      flowStatsFile << "  Throughput: " <<
        flow.second.rxBytes * 8.0 / (flow.second.timeLastRxPacket.GetSeconds () -
                                     flow.second.timeFirstTxPacket.GetSeconds ()) / 1e6 << " Mbps\n";
      flowStatsFile << "  Mean Delay: "
                    << (flow.second.delaySum.GetSeconds () / flow.second.rxPackets)
                    << " s\n";
      flowStatsFile << "  Packet Loss Rate: "
                    << 1.0 - double(flow.second.rxPackets) / double(flow.second.txPackets)
                    << "\n\n";
    }
  flowStatsFile.close ();

  Simulator::Destroy ();
  return 0;
}