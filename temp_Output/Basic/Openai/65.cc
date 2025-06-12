#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/applications-module.h"
#include "ns3/queue-disc.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

void
CwndTracer (uint32_t oldCwnd, uint32_t newCwnd)
{
  std::cout << Simulator::Now ().GetSeconds ()
            << "s CWND: " << oldCwnd << " -> " << newCwnd << std::endl;
}
void
RttTracer (Time oldRtt, Time newRtt)
{
  std::cout << Simulator::Now ().GetSeconds ()
            << "s RTT: " << newRtt.GetMilliSeconds () << " ms" << std::endl;
}
void
RxTracer (Ptr<Application> sink)
{
  Ptr<PacketSink> packetSink = DynamicCast<PacketSink> (sink);
  std::cout << Simulator::Now ().GetSeconds () << "s Throughput: "
            << packetSink->GetTotalRx () * 8.0 / 1e6 << " Mbps" << std::endl;
  Simulator::Schedule (Seconds (1.0), &RxTracer, sink);
}
void
QueueDiscDropTracer (Ptr<const QueueDiscItem> item)
{
  std::cout << Simulator::Now ().GetSeconds ()
            << "s QueueDiscDrop: " << item->GetPacket ()->GetUid () << std::endl;
}
void
QueueDiscMarkTracer (Ptr<const QueueDiscItem> item)
{
  std::cout << Simulator::Now ().GetSeconds ()
            << "s QueueDiscMark: " << item->GetPacket ()->GetUid () << std::endl;
}
void
QueueLengthTracer (uint32_t oldValue, uint32_t newValue)
{
  std::cout << Simulator::Now ().GetSeconds ()
            << "s QueueLength: " << newValue << std::endl;
}
int
main (int argc, char *argv[])
{
  bool enablePcap = true;
  double simStopTime = 10.0;
  uint32_t maxBytes = 10000000;

  CommandLine cmd;
  cmd.AddValue ("enablePcap", "Enable/disable PCAP tracing", enablePcap);
  cmd.AddValue ("simStopTime", "Simulation stop time", simStopTime);
  cmd.AddValue ("maxBytes", "MaxBytes for BulkSendHelper", maxBytes);
  cmd.Parse (argc, argv);

  NodeContainer nodes;
  nodes.Create (4);

  // Topology: n0---bottleneck---n1
  //           n2---------------n3
  NodeContainer n0n1 = NodeContainer (nodes.Get (0), nodes.Get (1));
  NodeContainer n2n3 = NodeContainer (nodes.Get (2), nodes.Get (3));
  NodeContainer bottleneck = NodeContainer (nodes.Get (1), nodes.Get (2));

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));
  NetDeviceContainer dev_n0n1 = p2p.Install (n0n1);

  p2p.SetDeviceAttribute ("DataRate", StringValue ("1Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("10ms"));
  NetDeviceContainer dev_bottleneck = p2p.Install (bottleneck);

  p2p.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));
  NetDeviceContainer dev_n2n3 = p2p.Install (n2n3);

  TrafficControlHelper tch;
  uint16_t handle = tch.SetRootQueueDisc ("ns3::FqCoDelQueueDisc");
  tch.AddInternalQueues (handle, "ns3::Queue", QueueSizeValue (QueueSize ("100p")));

  // Enable dynamic queue limits on bottleneck
#if NS3_VERSION_CODE >= 34100
  tch.SetQueueDiscAttribute ("AdaptQueueLimits", BooleanValue (true));
#endif
  tch.Install (dev_bottleneck);

  InternetStackHelper stack;
  stack.InstallAll ();

  Ipv4AddressHelper addr;
  addr.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer if_n0n1 = addr.Assign (dev_n0n1);

  addr.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer if_bottleneck = addr.Assign (dev_bottleneck);

  addr.SetBase ("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer if_n2n3 = addr.Assign (dev_n2n3);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // Applications
  // Ping from n0 to n3
  uint32_t packetSize = 1024;
  uint32_t numPings = 5;
  PingHelper pingHelper (if_n2n3.GetAddress (1));
  pingHelper.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  pingHelper.SetAttribute ("Size", UintegerValue (packetSize));
  pingHelper.SetAttribute ("Count", UintegerValue (numPings));
  ApplicationContainer pingApps = pingHelper.Install (nodes.Get (0));
  pingApps.Start (Seconds (0.5));
  pingApps.Stop (Seconds (simStopTime - 1));

  // BulkSend from n0 to n3 via bottleneck
  uint16_t port = 50000;
  BulkSendHelper bulkSender ("ns3::TcpSocketFactory", InetSocketAddress (if_n2n3.GetAddress (1), port));
  bulkSender.SetAttribute ("MaxBytes", UintegerValue (maxBytes));
  ApplicationContainer bulkApps = bulkSender.Install (nodes.Get (0));
  bulkApps.Start (Seconds (1.0));
  bulkApps.Stop (Seconds (simStopTime - 0.5));

  PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), port));
  ApplicationContainer sinkApps = sinkHelper.Install (nodes.Get (3));
  sinkApps.Start (Seconds (0.9));
  sinkApps.Stop (Seconds (simStopTime));

  // Tracing: bottleneck queue disc
  Ptr<NetDevice> bottleneckDev = dev_bottleneck.Get (0);
  Ptr<TrafficControlLayer> tc = bottleneck.Get (0)->GetObject<TrafficControlLayer> ();
  Ptr<QueueDisc> qdisc = tc->GetRootQueueDiscOnDevice (bottleneckDev);

  if (!qdisc)
    qdisc = TrafficControlHelper::GetInstalledQueueDisc (bottleneckDev);

  if (qdisc)
    {
      qdisc->TraceConnectWithoutContext ("Drop", MakeCallback (&QueueDiscDropTracer));
      qdisc->TraceConnectWithoutContext ("Mark", MakeCallback (&QueueDiscMarkTracer));
      if (qdisc->GetNInternalQueues () > 0)
        {
          Ptr<Queue<Packet> > queue = qdisc->GetInternalQueue (0);
          queue->TraceConnectWithoutContext ("PacketsInQueue", MakeCallback (&QueueLengthTracer));
        }
    }

  // TCP RTT, Cwnd, throughput traces
  Ptr<Application> sinkApp = sinkApps.Get (0);
  Simulator::Schedule (Seconds (1.0), &RxTracer, sinkApp);

  Config::Connect ("/NodeList/0/$ns3::TcpL4Protocol/SocketList/0/CongestionWindow", MakeCallback (&CwndTracer));
  Config::Connect ("/NodeList/0/$ns3::TcpL4Protocol/SocketList/0/RTT", MakeCallback (&RttTracer));

  if (enablePcap)
    {
      p2p.EnablePcapAll ("ns3-traffic-control");
    }

  Simulator::Stop (Seconds (simStopTime));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}