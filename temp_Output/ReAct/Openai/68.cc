#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ThreeNodeTcpBottleneckSimulation");

enum QueueDiscType
{
  QUEUE_DISC_PFIFO_FAST,
  QUEUE_DISC_ARED,
  QUEUE_DISC_CODEL,
  QUEUE_DISC_FQ_CODEL,
  QUEUE_DISC_PIE
};

static std::string
GetQueueDiscName(QueueDiscType type)
{
  switch (type)
    {
    case QUEUE_DISC_PFIFO_FAST: return "ns3::PfifoFastQueueDisc";
    case QUEUE_DISC_ARED:       return "ns3::AREDQueueDisc";
    case QUEUE_DISC_CODEL:      return "ns3::CoDelQueueDisc";
    case QUEUE_DISC_FQ_CODEL:   return "ns3::FqCoDelQueueDisc";
    case QUEUE_DISC_PIE:        return "ns3::PieQueueDisc";
    default:                    return "ns3::PfifoFastQueueDisc";
    }
}

void
TraceQueueLimits(Ptr<QueueDisc> queue, std::string filename)
{
  std::ofstream ofs(filename, std::ios_base::out | std::ios_base::trunc);
  ofs << "Time(s)\tLimit(Bytes)\n";
  ofs.close();
  queue->TraceConnectWithoutContext("Limit", MakeCallback([filename](uint32_t oldValue, uint32_t newValue) {
    std::ofstream ofs(filename, std::ios_base::out | std::ios_base::app);
    ofs << Simulator::Now().GetSeconds() << "\t" << newValue << "\n";
  }));
}

void
TraceBytesInQueue(Ptr<QueueDisc> queue, std::string filename)
{
  std::ofstream ofs(filename, std::ios_base::out | std::ios_base::trunc);
  ofs << "Time(s)\tBytesInQueue\n";
  ofs.close();
  queue->TraceConnectWithoutContext("BytesInQueue", MakeCallback([filename](uint32_t oldValue, uint32_t newValue) {
    std::ofstream ofs(filename, std::ios_base::out | std::ios_base::app);
    ofs << Simulator::Now().GetSeconds() << "\t" << newValue << "\n";
  }));
}

Ptr<RateErrorModel>
CreateRateErrorModel(double errorRate)
{
  Ptr<RateErrorModel> em = CreateObject<RateErrorModel>();
  em->SetAttribute("ErrorRate", DoubleValue(errorRate));
  em->SetAttribute("ErrorUnit", StringValue("ERROR_UNIT_PACKET"));
  return em;
}

double
CalculateGoodput(uint64_t bytes, double duration)
{
  return (bytes * 8.0 / duration) / 1e6; // Mbps
}

int main(int argc, char *argv[])
{
  // Default parameters, can be changed through command-line arguments
  std::string bottleneckBandwidth = "10Mbps";
  std::string bottleneckDelay = "10ms";
  QueueDiscType queueDiscType = QUEUE_DISC_CODEL;
  bool bqlEnabled = false;
  uint32_t queueDiscLimit = 1000;

  CommandLine cmd;
  cmd.AddValue("bottleneckBw", "Bandwidth of bottleneck link (e.g., 10Mbps)", bottleneckBandwidth);
  cmd.AddValue("bottleneckDelay", "Delay of bottleneck link (e.g., 10ms)", bottleneckDelay);
  cmd.AddValue("queueDisc", "Queue disc type: 0=PfifoFast,1=ARED,2=CoDel,3=FqCoDel,4=PIE", queueDiscType);
  cmd.AddValue("bql", "Enable Byte Queue Limits on bottleneck", bqlEnabled);
  cmd.AddValue("queueDiscLimit", "Queue disc limit", queueDiscLimit);
  cmd.Parse(argc, argv);

  // Create Nodes
  NodeContainer nodes;
  nodes.Create(3);
  Ptr<Node> n1 = nodes.Get(0);
  Ptr<Node> n2 = nodes.Get(1);
  Ptr<Node> n3 = nodes.Get(2);

  // Set up point-to-point links
  PointToPointHelper p2pAccess, p2pBottleneck;
  p2pAccess.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
  p2pAccess.SetChannelAttribute("Delay", StringValue("0.1ms"));
  p2pAccess.SetQueue("ns3::DropTailQueue", "MaxSize", StringValue("100p"));

  p2pBottleneck.SetDeviceAttribute("DataRate", StringValue(bottleneckBandwidth));
  p2pBottleneck.SetChannelAttribute("Delay", StringValue(bottleneckDelay));
  p2pBottleneck.SetQueue("ns3::DropTailQueue", "MaxSize", StringValue("100p"));

  // Install devices and channels
  NetDeviceContainer devAccess = p2pAccess.Install(n1, n2);
  NetDeviceContainer devBottleneck = p2pBottleneck.Install(n2, n3);

  // Enable Byte Queue Limits if set
  if (bqlEnabled)
    {
      Config::Set ("/NodeList/*/DeviceList/*/$ns3::PointToPointNetDevice/TxQueue/Mode", StringValue("QUEUE_MODE_BYTES"));
      Config::Set ("/NodeList/*/DeviceList/*/$ns3::PointToPointNetDevice/TxQueue/MaxSize", StringValue("100000B"));
    }

  // Install Internet stack
  InternetStackHelper stack;
  stack.Install(nodes);

  // Assign IP addresses
  Ipv4AddressHelper ipv4Access, ipv4Bottleneck;
  ipv4Access.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer ifAccess = ipv4Access.Assign(devAccess);

  ipv4Bottleneck.SetBase("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer ifBottleneck = ipv4Bottleneck.Assign(devBottleneck);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // Traffic Control: Attach queue disc to bottleneck
  TrafficControlHelper tch;
  if(queueDiscType == QUEUE_DISC_PFIFO_FAST)
    {
      tch.SetRootQueueDisc("ns3::PfifoFastQueueDisc", "MaxSize", StringValue(std::to_string(queueDiscLimit) + "p"));
    }
  else if(queueDiscType == QUEUE_DISC_ARED)
    {
      tch.SetRootQueueDisc("ns3::AREDQueueDisc", "MaxSize", StringValue(std::to_string(queueDiscLimit) + "p"));
    }
  else if(queueDiscType == QUEUE_DISC_CODEL)
    {
      tch.SetRootQueueDisc("ns3::CoDelQueueDisc", "MaxSize", StringValue(std::to_string(queueDiscLimit) + "p"));
    }
  else if(queueDiscType == QUEUE_DISC_FQ_CODEL)
    {
      tch.SetRootQueueDisc("ns3::FqCoDelQueueDisc", "MaxSize", StringValue(std::to_string(queueDiscLimit) + "p"));
    }
  else if(queueDiscType == QUEUE_DISC_PIE)
    {
      tch.SetRootQueueDisc("ns3::PieQueueDisc", "MaxSize", StringValue(std::to_string(queueDiscLimit) + "p"));
    }

  QueueDiscContainer qdiscs = tch.Install(devBottleneck.Get(0));
  Ptr<QueueDisc> bottleneckQueueDisc = qdiscs.Get(0);

  // Tracing: queue limits and bytes in queue
  TraceQueueLimits(bottleneckQueueDisc, "queue-limits.tr");
  TraceBytesInQueue(bottleneckQueueDisc, "bytes-in-queue.tr");

  // Applications: TCP bidirectional flows
  uint16_t port1 = 5000;
  uint16_t port2 = 5001;

  // TCP n1 -> n3
  Address sinkAddress1 (InetSocketAddress (ifBottleneck.GetAddress(1), port1));
  PacketSinkHelper sinkHelper1 ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny(), port1));
  ApplicationContainer sinkApp1 = sinkHelper1.Install(n3);
  sinkApp1.Start(Seconds(0.5));
  sinkApp1.Stop(Seconds(20.0));

  OnOffHelper clientHelper1 ("ns3::TcpSocketFactory", sinkAddress1);
  clientHelper1.SetAttribute ("DataRate", StringValue ("200Mbps"));
  clientHelper1.SetAttribute ("PacketSize", UintegerValue (1448));
  clientHelper1.SetAttribute ("StartTime", TimeValue (Seconds (1.0)));
  clientHelper1.SetAttribute ("StopTime", TimeValue (Seconds (19.0)));
  ApplicationContainer clientApp1 = clientHelper1.Install(n1);

  // TCP n3 -> n1
  Address sinkAddress2 (InetSocketAddress (ifAccess.GetAddress(0), port2));
  PacketSinkHelper sinkHelper2 ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny(), port2));
  ApplicationContainer sinkApp2 = sinkHelper2.Install(n1);
  sinkApp2.Start(Seconds(0.5));
  sinkApp2.Stop(Seconds(20.0));

  OnOffHelper clientHelper2 ("ns3::TcpSocketFactory", sinkAddress2);
  clientHelper2.SetAttribute ("DataRate", StringValue ("200Mbps"));
  clientHelper2.SetAttribute ("PacketSize", UintegerValue (1448));
  clientHelper2.SetAttribute ("StartTime", TimeValue (Seconds (1.2)));
  clientHelper2.SetAttribute ("StopTime", TimeValue (Seconds (19.0)));
  ApplicationContainer clientApp2 = clientHelper2.Install(n3);

  // ICMP Ping: n1 -> n3
  V4PingHelper ping(ifBottleneck.GetAddress(1));
  ping.SetAttribute("Interval", TimeValue(Seconds(0.2)));
  ping.SetAttribute("Verbose", BooleanValue(true));
  ApplicationContainer pingApps = ping.Install(n1);
  pingApps.Start(Seconds(2.0));
  pingApps.Stop(Seconds(19.0));

  // Goodput monitoring: will log time, bytes received, goodput for both dirs
  std::ofstream goodputLog;
  goodputLog.open("goodput.tr", std::ios_base::out | std::ios_base::trunc);
  goodputLog << "Time(s)\tn1-to-n3(Mbps)\tn3-to-n1(Mbps)\n";
  goodputLog.close();

  uint64_t lastRxBytes1 = 0;
  uint64_t lastRxBytes2 = 0;
  double lastTime = 1.0;

  Ptr<PacketSink> sink1 = DynamicCast<PacketSink>(sinkApp1.Get(0));
  Ptr<PacketSink> sink2 = DynamicCast<PacketSink>(sinkApp2.Get(0));

  void(*goodputLogger)(void) = nullptr;
  goodputLogger = [&]()
  {
    double now = Simulator::Now().GetSeconds();
    double interval = now - lastTime;
    uint64_t rxBytes1 = sink1->GetTotalRx();
    uint64_t rxBytes2 = sink2->GetTotalRx();
    double goodput1 = CalculateGoodput(rxBytes1 - lastRxBytes1, interval);
    double goodput2 = CalculateGoodput(rxBytes2 - lastRxBytes2, interval);
    std::ofstream out("goodput.tr", std::ios_base::out | std::ios_base::app);
    out << now << "\t" << goodput1 << "\t" << goodput2 << "\n";
    out.close();
    lastRxBytes1 = rxBytes1;
    lastRxBytes2 = rxBytes2;
    lastTime = now;
    Simulator::Schedule(Seconds(1.0), goodputLogger);
  };

  Simulator::Schedule(Seconds(2.0), goodputLogger);

  // FlowMonitor
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  Simulator::Stop(Seconds(20.0));
  Simulator::Run();

  // FlowMonitor: output statistics
  monitor->CheckForLostPackets();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

  std::ofstream flowStats("flow-stats.tr", std::ios_base::out | std::ios_base::trunc);
  flowStats << "FlowID\tSrcAddr\tDstAddr\tTxBytes\tRxBytes\tLostPackets\tThroughput(Mbps)\tMeanDelay(s)\n";
  for (auto const &flow : stats)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flow.first);
      double duration = (flow.second.timeLastRxPacket.GetSeconds () - flow.second.timeFirstTxPacket.GetSeconds ());
      double throughput = duration > 0 ? (flow.second.rxBytes * 8.0 / duration / 1e6) : 0.0;
      double meanDelay = (flow.second.rxPackets > 0) ? (flow.second.delaySum.GetSeconds() / flow.second.rxPackets) : 0.0;
      flowStats << flow.first << "\t"
        << t.sourceAddress << "\t"
        << t.destinationAddress << "\t"
        << flow.second.txBytes << "\t"
        << flow.second.rxBytes << "\t"
        << flow.second.lostPackets << "\t"
        << throughput << "\t"
        << meanDelay << "\n";
    }
  flowStats.close();

  Simulator::Destroy ();
  return 0;
}