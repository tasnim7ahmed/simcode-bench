#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("RedTcDemo");

static std::vector<uint32_t> tcQueueLens;
static std::vector<uint32_t> netdevQueueLens;
static Ptr<QueueDisc> redQueue;
static Ptr<Queue<Packet>> netdevQueue;
static Time lastEnqueueTime;
static std::vector<Time> sojournTimes;

void
TcQueueDiscWatcher ()
{
  if (redQueue)
    {
      tcQueueLens.push_back (redQueue->GetCurrentSize ().GetValue ());
    }
  Simulator::Schedule (MilliSeconds (10), &TcQueueDiscWatcher);
}

void
NetdevQueueWatcher ()
{
  if (netdevQueue)
    {
      netdevQueueLens.push_back (netdevQueue->GetNPackets ());
    }
  Simulator::Schedule (MilliSeconds (10), &NetdevQueueWatcher);
}

void
PacketEnqueueCb (Ptr<const QueueDiscItem> item)
{
  item->GetPacket ()->AddByteTag (TimestampTag (Simulator::Now ().GetNanoSeconds ()));
}

void
PacketDequeueCb (Ptr<const QueueDiscItem> item)
{
  TimestampTag tag;
  if (item->GetPacket ()->FindFirstMatchingByteTag (tag))
    {
      Time enqueueTime = NanoSeconds (tag.GetTimestamp ());
      Time sojourn = Simulator::Now () - enqueueTime;
      sojournTimes.push_back (sojourn);
    }
}

int
main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  NodeContainer nodes;
  nodes.Create (2);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer devices = p2p.Install (nodes);

  InternetStackHelper stack;
  stack.Install (nodes);

  TrafficControlHelper tch;
  uint16_t handle = tch.SetRootQueueDisc ("ns3::RedQueueDisc");
  tch.AddQueueDiscClasses (handle, 1, "ns3::QueueDiscClass");
  tch.AddPacketFilter (handle, "ns3::PFifoFastPacketFilter");
  QueueDiscContainer qdiscs = tch.Install (devices);

  redQueue = qdiscs.Get (0);

  Ptr<PointToPointNetDevice> ptpNetDev = DynamicCast<PointToPointNetDevice> (devices.Get (0));
  netdevQueue = ptpNetDev->GetQueue ();

  // Track sojourn time using Tags
  redQueue->TraceConnectWithoutContext ("Enqueue", MakeCallback (&PacketEnqueueCb));
  redQueue->TraceConnectWithoutContext ("Dequeue", MakeCallback (&PacketDequeueCb));

  Simulator::ScheduleNow (&TcQueueDiscWatcher);
  Simulator::ScheduleNow (&NetdevQueueWatcher);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = ipv4.Assign (devices);

  uint16_t port = 50000;
  Address sinkAddress (InetSocketAddress (interfaces.GetAddress (1), port));
  PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory", sinkAddress);
  ApplicationContainer sinkApp = sinkHelper.Install (nodes.Get (1));
  sinkApp.Start (Seconds (0.0));
  sinkApp.Stop (Seconds (10.0));

  OnOffHelper client ("ns3::TcpSocketFactory", sinkAddress);
  client.SetAttribute ("DataRate", StringValue ("8Mbps"));
  client.SetAttribute ("PacketSize", UintegerValue (1024));
  client.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
  client.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
  ApplicationContainer clientApp = client.Install (nodes.Get (0));
  clientApp.Start (Seconds (1.0));
  clientApp.Stop (Seconds (9.0));

  FlowMonitorHelper flowmonHelper;
  Ptr<FlowMonitor> flowmon = flowmonHelper.InstallAll ();

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();

  // Print statistics
  uint32_t tcSum = 0, tcCnt = tcQueueLens.size ();
  for (uint32_t len : tcQueueLens) tcSum += len;

  uint32_t netdevSum = 0, netdevCnt = netdevQueueLens.size ();
  for (uint32_t len : netdevQueueLens) netdevSum += len;

  double avgTcQueue = tcCnt ? (double)tcSum / tcCnt : 0.0;
  double avgNetdevQueue = netdevCnt ? (double)netdevSum / netdevCnt : 0.0;

  double sumSojourn = 0, cntSojourn = sojournTimes.size ();
  for (Time t : sojournTimes) sumSojourn += t.GetMilliSeconds ();
  double avgSojourn = cntSojourn ? sumSojourn / cntSojourn : 0.0;

  std::cout << "TrafficControl (Red) Queue Average Length: " << avgTcQueue << " pkts\n";
  std::cout << "NetDevice Queue Average Length: " << avgNetdevQueue << " pkts\n";
  std::cout << "Average sojourn time at TrafficControl: " << avgSojourn << " ms\n";

  flowmon->CheckForLostPackets ();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmonHelper.GetClassifier ());
  FlowMonitor::FlowStatsContainer stats = flowmon->GetFlowStats ();

  for (auto &flow : stats)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (flow.first);
      std::cout << "\nFlow " << flow.first << " (" << t.sourceAddress << " -> " 
                << t.destinationAddress << ")\n";
      double time = (flow.second.timeLastRxPacket.GetSeconds () - flow.second.timeFirstTxPacket.GetSeconds ());
      double throughput = time > 0 ? flow.second.rxBytes * 8.0 / time / 1e6 : 0;
      std::cout << "  Throughput: " << throughput << " Mbps\n";
      std::cout << "  Mean delay: "
                << (flow.second.delaySum.GetSeconds () / flow.second.rxPackets) * 1000 << " ms\n";
      std::cout << "  Mean jitter: "
                << (flow.second.jitterSum.GetSeconds () / flow.second.rxPackets) * 1000 << " ms\n";
    }
  Simulator::Destroy ();
  return 0;
}

class TimestampTag : public Tag
{
public:
  TimestampTag() : m_timestamp (0) {}
  explicit TimestampTag (uint64_t t) : m_timestamp (t) {}

  static TypeId GetTypeId (void)
  {
    static TypeId tid = TypeId ("TimestampTag")
      .SetParent<Tag> ()
      .AddConstructor<TimestampTag> ();
    return tid;
  }
  virtual TypeId GetInstanceTypeId (void) const
  {
    return GetTypeId ();
  }
  virtual uint32_t GetSerializedSize (void) const
  {
    return 8;
  }
  virtual void Serialize (TagBuffer i) const
  {
    i.WriteU64 (m_timestamp);
  }
  virtual void Deserialize (TagBuffer i)
  {
    m_timestamp = i.ReadU64 ();
  }
  virtual void Print (std::ostream &os) const
  {
    os << m_timestamp;
  }
  void SetTimestamp(uint64_t ts)
  {
    m_timestamp = ts;
  }
  uint64_t GetTimestamp() const
  {
    return m_timestamp;
  }
private:
  uint64_t m_timestamp;
};