#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/queue-disc-module.h"

using namespace ns3;

// Trace Congestion Window
static void 
CwndChange (Ptr<OutputStreamWrapper> stream, uint32_t oldCwnd, uint32_t newCwnd)
{
  *stream->GetStream () << Simulator::Now ().GetSeconds () << "\t" << newCwnd << std::endl;
}

// Trace Throughput
static void
CalculateThroughput (Ptr<Application> app, Ptr<OutputStreamWrapper> stream, uint64_t lastTotalRx)
{
  Ptr<PacketSink> sink = DynamicCast<PacketSink> (app);
  uint64_t cur = sink->GetTotalRx ();
  double throughput = (cur - lastTotalRx) * 8.0 / 1e6; // Mbit/s in last second
  *stream->GetStream () << Simulator::Now ().GetSeconds () << "\t" << throughput << std::endl;
  Simulator::Schedule (Seconds (1.0), &CalculateThroughput, app, stream, cur);
}

// Trace Queue Size
static void
QueueSizeTrace (Ptr<OutputStreamWrapper> stream, uint32_t oldVal, uint32_t newVal)
{
  *stream->GetStream () << Simulator::Now ().GetSeconds () << "\t" << newVal << std::endl;
}

// Force BBR PROBE_RTT
void
ForceBbrProbeRTT (Ptr<Socket> tcpSocket)
{
  tcpSocket->SetSockOpt (Socket::NS3_SOCK_OPT_USER, 1, 1); // Pretend to trigger PROBE_RTT; see below
  // There is currently no public API in ns-3.41 to directly force PROBE_RTT state in BBR.
  // In practice, applications might reset EPOCH or manipulate RTT values; here, we reduce the congestion window.
  Ptr<TcpSocketBase> tcpBase = DynamicCast<TcpSocketBase> (tcpSocket);
  if (tcpBase)
    {
      tcpBase->SetCwnd (tcpBase->GetSegmentSize () * 4);
    }
  Simulator::Schedule (Seconds (10.0), &ForceBbrProbeRTT, tcpSocket);
}

int 
main (int argc, char *argv[])
{
  Time::SetResolution (Time::NS);
  LogComponentEnable ("BulkSendApplication", LOG_LEVEL_INFO);
  LogComponentEnable ("PacketSink", LOG_LEVEL_INFO);
  Config::SetDefault ("ns3::TcpL4Protocol::SocketType", StringValue ("ns3::TcpBbr"));

  // Nodes: 0=Sender, 1=R1, 2=R2, 3=Receiver
  NodeContainer nodes;
  nodes.Create (4);

  // Point-to-point link helpers
  PointToPointHelper p2pSender_R1, p2pR1_R2, p2pR2_Receiver;
  p2pSender_R1.SetDeviceAttribute ("DataRate", StringValue ("1000Mbps"));
  p2pSender_R1.SetChannelAttribute ("Delay", StringValue ("5ms"));

  p2pR1_R2.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  p2pR1_R2.SetChannelAttribute ("Delay", StringValue ("10ms"));
  // Use a DropTail queue (default): we will trace queue size.

  p2pR2_Receiver.SetDeviceAttribute ("DataRate", StringValue ("1000Mbps"));
  p2pR2_Receiver.SetChannelAttribute ("Delay", StringValue ("5ms"));

  // Install devices
  NetDeviceContainer dSender_R1 = p2pSender_R1.Install (nodes.Get (0), nodes.Get (1));
  NetDeviceContainer dR1_R2 = p2pR1_R2.Install (nodes.Get (1), nodes.Get (2));
  NetDeviceContainer dR2_Receiver = p2pR2_Receiver.Install (nodes.Get (2), nodes.Get (3));

  // Stack
  InternetStackHelper stack;
  stack.InstallAll ();

  // Assign IPs
  Ipv4AddressHelper ip1, ip2, ip3;
  ip1.SetBase ("10.1.1.0", "255.255.255.0");
  ip2.SetBase ("10.1.2.0", "255.255.255.0");
  ip3.SetBase ("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer i1 = ip1.Assign (dSender_R1);
  Ipv4InterfaceContainer i2 = ip2.Assign (dR1_R2);
  Ipv4InterfaceContainer i3 = ip3.Assign (dR2_Receiver);

  // Enable routing
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // Applications: BulkSend as Sender, PacketSink as Receiver
  uint16_t port = 50000;
  Address sinkAddress (InetSocketAddress (i3.GetAddress (1), port));
  PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory", sinkAddress);
  ApplicationContainer sinkApp = sinkHelper.Install (nodes.Get (3));
  sinkApp.Start (Seconds (0.0));
  sinkApp.Stop (Seconds (100.0));

  BulkSendHelper source ("ns3::TcpSocketFactory", sinkAddress);
  source.SetAttribute ("MaxBytes", UintegerValue (0));
  ApplicationContainer srcApp = source.Install (nodes.Get (0));
  srcApp.Start (Seconds (1.0));
  srcApp.Stop (Seconds (100.0));

  // Enable pcap tracing
  p2pSender_R1.EnablePcapAll ("p2p-sender-r1");
  p2pR1_R2.EnablePcapAll ("p2p-r1-r2");
  p2pR2_Receiver.EnablePcapAll ("p2p-r2-receiver");

  // Congestion window trace
  Ptr<Socket> tcpSocket = Socket::CreateSocket (nodes.Get (0), TcpSocketFactory::GetTypeId ());
  tcpSocket->Bind ();
  tcpSocket->Connect (sinkAddress);

  AsciiTraceHelper asciiCwnd;
  Ptr<OutputStreamWrapper> streamCwnd = asciiCwnd.CreateFileStream ("cwnd.txt");
  tcpSocket->TraceConnectWithoutContext ("CongestionWindow", MakeBoundCallback (&CwndChange, streamCwnd));

  // Throughput trace (done with PacketSink)
  AsciiTraceHelper asciiTput;
  Ptr<OutputStreamWrapper> streamTput = asciiTput.CreateFileStream ("throughput.txt");
  Simulator::Schedule (Seconds (1.1), &CalculateThroughput, sinkApp.Get (0), streamTput, 0);

  // Queue size trace on bottleneck link (R1->R2, on R1 side)
  Ptr<NetDevice> r1dev = dR1_R2.Get (0);
  Ptr<PointToPointNetDevice> ptpND = DynamicCast<PointToPointNetDevice> (r1dev);
  Ptr<Queue<Packet>> queue = ptpND->GetQueue ();

  AsciiTraceHelper asciiQueue;
  Ptr<OutputStreamWrapper> streamQueue = asciiQueue.CreateFileStream ("queue-size.txt");
  if (queue)
    {
      queue->TraceConnectWithoutContext ("PacketsInQueue", MakeBoundCallback (&QueueSizeTrace, streamQueue));
    }

  // Set TCP BBR-specific attribute (ensuring minimal probe RTT interval)
  Config::SetDefault ("ns3::TcpBbr::ProbeRttInterval", TimeValue (Seconds (10.0)));
  Config::SetDefault ("ns3::TcpBbr::ProbeRttDuration", TimeValue (Seconds (0.2))); // ~200ms

  // Force BBR into PROBE_RTT phase every 10 seconds: 
  Simulator::Schedule (Seconds (10.0), &ForceBbrProbeRTT, tcpSocket);

  Simulator::Stop (Seconds (100.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}