#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("RedQueueDiscDemo");

static void
PrintQueueStats (Ptr<QueueDisc> queue)
{
  QueueDisc::Stats st = queue->GetStats ();
  std::cout << Simulator::Now ().GetSeconds ()
            << "s [TrafficControl] Packets (enq/deq/dropped overlimit): "
            << st.nPacketsEnqueued << "/" << st.nPacketsDequeued << "/" << st.nPacketsDroppedOverLimit
            << " Current Qlen: " << queue->GetCurrentSize ().GetValue () << std::endl;
  Simulator::Schedule (Seconds (0.1), &PrintQueueStats, queue);
}

static void
PrintNetDeviceQueueStats (Ptr<NetDevice> device)
{
  Ptr<PointToPointNetDevice> ptpDevice = DynamicCast<PointToPointNetDevice> (device);
  if (ptpDevice)
    {
      Ptr<Queue<Packet>> q = ptpDevice->GetQueue ();
      if (q)
        {
          std::cout << Simulator::Now ().GetSeconds ()
                    << "s [Device] NetDevice Queue size: "
                    << q->GetNPackets () << std::endl;
        }
    }
  Simulator::Schedule (Seconds (0.1), &PrintNetDeviceQueueStats, device);
}

class SojournTimeTracer
{
public:
  void Enqueue (Ptr<const QueueDiscItem> item)
  {
    m_times[item->GetPacket ()->GetUid ()] = Simulator::Now ();
  }

  void Dequeue (Ptr<const QueueDiscItem> item)
  {
    uint32_t uid = item->GetPacket ()->GetUid ();
    if (m_times.find (uid) != m_times.end ())
      {
        Time sojourn = Simulator::Now () - m_times[uid];
        m_sojournSamples.push_back (sojourn.GetSeconds ());
        m_times.erase (uid);
      }
  }

  double GetMeanSojournTime () const
  {
    if (m_sojournSamples.empty ()) return 0.0;
    double sum = 0.0;
    for (auto t : m_sojournSamples) sum += t;
    return sum / m_sojournSamples.size ();
  }

  void Attach (Ptr<QueueDisc> qdisc)
  {
    m_enqueueConn = qdisc->TraceConnectWithoutContext ("Enqueue", MakeCallback (&SojournTimeTracer::Enqueue, this));
    m_dequeueConn = qdisc->TraceConnectWithoutContext ("Dequeue", MakeCallback (&SojournTimeTracer::Dequeue, this));
  }
private:
  std::map<uint32_t, Time> m_times;
  std::vector<double> m_sojournSamples;
  TracedCallback<Ptr<const QueueDiscItem>> m_enqueueConn, m_dequeueConn;
};

int
main (int argc, char *argv[])
{
  Time::SetResolution (Time::NS);

  NodeContainer nodes;
  nodes.Create (2);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("5ms"));
  p2p.SetQueue ("ns3::DropTailQueue<Packet>", "MaxSize", StringValue ("100p"));

  NetDeviceContainer devices = p2p.Install (nodes);

  TrafficControlHelper tch;
  uint16_t handle = tch.SetRootQueueDisc ("ns3::RedQueueDisc",
                                          "MinTh", DoubleValue (5),
                                          "MaxTh", DoubleValue (15),
                                          "LinkBandwidth", StringValue ("10Mbps"),
                                          "LinkDelay", StringValue ("5ms"));
  QueueDiscContainer qdiscs = tch.Install (devices);

  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  uint16_t port = 50000;
  Address sinkAddress (InetSocketAddress (interfaces.GetAddress (1), port));
  PacketSinkHelper packetSinkHelper ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), port));
  ApplicationContainer sinkApp = packetSinkHelper.Install (nodes.Get (1));
  sinkApp.Start (Seconds (0.0));
  sinkApp.Stop (Seconds (10.0));

  OnOffHelper source ("ns3::TcpSocketFactory", sinkAddress);
  source.SetAttribute ("DataRate", StringValue ("5Mbps"));
  source.SetAttribute ("PacketSize", UintegerValue (1000));
  source.SetAttribute ("StartTime", TimeValue (Seconds (0.1)));
  source.SetAttribute ("StopTime", TimeValue (Seconds (9.9)));

  ApplicationContainer sourceApp = source.Install (nodes.Get (0));

  Simulator::Schedule (Seconds (0.1), &PrintQueueStats, qdiscs.Get (0));
  Simulator::Schedule (Seconds (0.1), &PrintNetDeviceQueueStats, devices.Get (0));

  SojournTimeTracer sojournTracer;
  sojournTracer.Attach (qdiscs.Get (0));

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();

  // Queue statistics and mean sojourn time
  std::cout << "*** Simulation finished ***" << std::endl;
  std::cout << "Mean packet sojourn time in queue disc: "
            << sojournTracer.GetMeanSojournTime () << " s" << std::endl;

  // FlowMonitor stats
  monitor->CheckForLostPackets ();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats ();
  for (auto it = stats.begin (); it != stats.end (); ++it)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (it->first);
      std::cout << "Flow " << it->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")" << std::endl;
      std::cout << "  Tx Packets: " << it->second.txPackets << std::endl;
      std::cout << "  Rx Packets: " << it->second.rxPackets << std::endl;
      double throughput = it->second.rxBytes * 8.0 / (it->second.timeLastRxPacket.GetSeconds () - it->second.timeFirstTxPacket.GetSeconds ()) / 1e6;
      std::cout << "  Throughput: " << throughput << " Mbps" << std::endl;
      double meanDelay = (it->second.rxPackets > 0) ? it->second.delaySum.GetSeconds () / it->second.rxPackets : 0.0;
      std::cout << "  Mean delay: " << meanDelay << " s" << std::endl;
      double meanJitter = (it->second.rxPackets > 1) ? it->second.jitterSum.GetSeconds () / (it->second.rxPackets - 1) : 0.0;
      std::cout << "  Mean jitter: " << meanJitter << " s" << std::endl;
    }

  Simulator::Destroy ();
  return 0;
}