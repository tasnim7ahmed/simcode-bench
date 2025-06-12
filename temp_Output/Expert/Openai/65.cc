#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/queue-disc.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

void
QueueDiscTraces (Ptr<QueueDisc> queue)
{
  queue->TraceConnectWithoutContext ("PacketsInQueue", MakeCallback ([] (uint32_t oldVal, uint32_t newVal) {
    NS_LOG_UNCOND (Simulator::Now ().GetSeconds () << "s Queue Length: " << newVal);
  }));
  queue->TraceConnectWithoutContext ("Drop", MakeCallback ([] (Ptr<const QueueDiscItem> item) {
    NS_LOG_UNCOND (Simulator::Now ().GetSeconds () << "s Queue Drop " << item->GetPacket ()->GetUid ());
  }));
  queue->TraceConnectWithoutContext ("Mark", MakeCallback ([] (Ptr<const QueueDiscItem> item) {
    NS_LOG_UNCOND (Simulator::Now ().GetSeconds () << "s Packet Marked " << item->GetPacket ()->GetUid ());
  }));
}

void
CwndTracer (uint32_t oldCwnd, uint32_t newCwnd)
{
  NS_LOG_UNCOND (Simulator::Now ().GetSeconds () << "s CWND: " << newCwnd);
}

void
RttTracer (Time oldRtt, Time newRtt)
{
  NS_LOG_UNCOND (Simulator::Now ().GetSeconds () << "s RTT: " << newRtt.GetMilliSeconds () << " ms");
}

void
ThroughputTracer (Ptr<Application> app, Ptr<PacketSink> sink)
{
  double throughput = (sink->GetTotalRx () * 8.0) / Simulator::Now ().GetSeconds () / 1e6;
  NS_LOG_UNCOND (Simulator::Now ().GetSeconds () << "s Throughput: " << throughput << " Mbps");
  Simulator::Schedule (Seconds (1.0), &ThroughputTracer, app, sink);
}

int
main (int argc, char *argv[])
{
  bool enablePcap = true;
  bool enableDctcp = false;
  uint32_t maxBytes = 10 * 1024 * 1024;
  double simTime = 20.0;

  CommandLine cmd;
  cmd.AddValue ("enablePcap", "Enable PCAP tracing", enablePcap);
  cmd.AddValue ("enableDctcp", "Enable DCTCP TCP variant", enableDctcp);
  cmd.AddValue ("maxBytes", "Maximum bytes to send by BulkSend", maxBytes);
  cmd.AddValue ("simTime", "Simulation time", simTime);
  cmd.Parse (argc, argv);

  LogComponentEnable ("BulkSendApplication", LOG_LEVEL_INFO);
  LogComponentEnable ("PacketSink", LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create (4);

  // Topology: n0--n2--n3
  //                 |
  //                n1

  PointToPointHelper p2pN0N2, p2pN1N2, p2pN2N3;
  p2pN0N2.SetDeviceAttribute ("DataRate", StringValue ("100Mbps"));
  p2pN0N2.SetChannelAttribute ("Delay", StringValue ("2ms"));
  p2pN1N2.SetDeviceAttribute ("DataRate", StringValue ("100Mbps"));
  p2pN1N2.SetChannelAttribute ("Delay", StringValue ("2ms"));
  p2pN2N3.SetDeviceAttribute ("DataRate", StringValue ("10Mbps")); // bottleneck
  p2pN2N3.SetChannelAttribute ("Delay", StringValue ("10ms"));

  NetDeviceContainer d0d2 = p2pN0N2.Install (nodes.Get (0), nodes.Get (2));
  NetDeviceContainer d1d2 = p2pN1N2.Install (nodes.Get (1), nodes.Get (2));
  NetDeviceContainer d2d3 = p2pN2N3.Install (nodes.Get (2), nodes.Get (3));

  // Traffic Control: attach FqCoDel to the bottleneck link (n2->n3)
  TrafficControlHelper tch;
  uint16_t handle = tch.SetRootQueueDisc ("ns3::FqCoDelQueueDisc");
  tch.AddQueueDiscClasses (handle, 1, "ns3::QueueDiscClass", "ClassId", UintegerValue (0));
  tch.SetQueueLimits (handle, "ns3::DynamicQueueLimits", "MinLimit", UintegerValue (100), "MaxLimit", UintegerValue (2000));
  QueueDiscContainer qd = tch.Install (d2d3);

  // Optional: traffic control for other links as well if needed
  TrafficControlHelper tchA;
  tchA.SetRootQueueDisc ("ns3::FqCoDelQueueDisc");
  tchA.Install (d0d2);
  tchA.Install (d1d2);

  InternetStackHelper stack;
  stack.Install (nodes);

  // Explicitly enable traffic-control layer
  Ipv4AddressHelper address;
  Ipv4InterfaceContainer i0i2, i1i2, i2i3;

  address.SetBase ("10.0.0.0", "255.255.255.0");
  i0i2 = address.Assign (d0d2);

  address.SetBase ("10.0.1.0", "255.255.255.0");
  i1i2 = address.Assign (d1d2);

  address.SetBase ("10.0.2.0", "255.255.255.0");
  i2i3 = address.Assign (d2d3);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // Ping: n0->n3
  PingHelper ping (i2i3.GetAddress (1));
  ping.SetAttribute ("Verbose", BooleanValue (true));
  ApplicationContainer pingApps = ping.Install (nodes.Get (0));
  pingApps.Start (Seconds (1.0));
  pingApps.Stop (Seconds (simTime - 1));

  // TCP BulkSend: n1 (client) -> n3 (server)
  uint16_t sinkPort = 8080;
  Address sinkAddress (InetSocketAddress (i2i3.GetAddress (1), sinkPort));

  PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), sinkPort));
  ApplicationContainer sinkApp = sinkHelper.Install (nodes.Get (3));
  sinkApp.Start (Seconds (0.0));
  sinkApp.Stop (Seconds (simTime));

  BulkSendHelper sourceHelper ("ns3::TcpSocketFactory", sinkAddress);
  sourceHelper.SetAttribute ("MaxBytes", UintegerValue (maxBytes));
  ApplicationContainer sourceApp = sourceHelper.Install (nodes.Get (1));
  sourceApp.Start (Seconds (2.0));
  sourceApp.Stop (Seconds (simTime));

  // PacketSink for client (if wanted, here for completeness)
  PacketSinkHelper clientSinkHelper ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), sinkPort + 1));
  ApplicationContainer clientSinkApp = clientSinkHelper.Install (nodes.Get (1));
  clientSinkApp.Start (Seconds (0.0));
  clientSinkApp.Stop (Seconds (simTime));

  // Tracing on queue disc at bottleneck
  Ptr<QueueDisc> queueDisc = qd.Get (0);
  QueueDiscTraces (queueDisc);

  // TCP tracing: RTT, CWND, etc.
  Config::ConnectWithoutContext ("/NodeList/1/$ns3::TcpL4Protocol/SocketList/0/CongestionWindow", MakeCallback (&CwndTracer));
  Config::ConnectWithoutContext ("/NodeList/1/$ns3::TcpL4Protocol/SocketList/0/RTT", MakeCallback (&RttTracer));
  Ptr<Application> bulkSendApp = sourceApp.Get (0);
  Ptr<PacketSink> sink = DynamicCast<PacketSink> (sinkApp.Get (0));
  Simulator::Schedule (Seconds (2.5), &ThroughputTracer, bulkSendApp, sink);

  // If DCTCP is enabled, set on BulkSend socket
  if (enableDctcp)
    {
      Config::SetDefault ("ns3::TcpL4Protocol::SocketType", StringValue ("ns3::TcpDctcp"));
      Config::ConnectWithoutContext ("/NodeList/1/$ns3::TcpL4Protocol/SocketList/0/ECN", MakeCallback ([] (bool oldVal, bool newVal) {
        NS_LOG_UNCOND (Simulator::Now ().GetSeconds () << "s ECN: " << newVal);
      }));
    }

  if (enablePcap)
    {
      p2pN2N3.EnablePcapAll ("p2p-n2n3", false);
      p2pN0N2.EnablePcapAll ("p2p-n0n2", false);
      p2pN1N2.EnablePcapAll ("p2p-n1n2", false);
    }

  Simulator::Stop (Seconds (simTime));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}