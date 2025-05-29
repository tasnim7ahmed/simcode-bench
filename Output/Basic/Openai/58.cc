#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/queue-disc-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("TcpBbrBottleneckSim");

Ptr<OutputStreamWrapper> cwndStream;
Ptr<OutputStreamWrapper> thrStream;
Ptr<OutputStreamWrapper> queueStream;

uint32_t lastTotalRx = 0;
Time lastRxTime = Seconds (0.0);
Ptr<TcpSocketBase> g_socket;

Ptr<TcpSocketBase> GetTcpSocketFromNode (Ptr<Node> node)
{
  for (uint32_t i = 0; i < node->GetNApplications (); ++i)
    {
      Ptr<Application> app = node->GetApplication (i);
      Ptr<BulkSendApplication> bsa = DynamicCast<BulkSendApplication>(app);
      if (bsa)
        {
          Ptr<Socket> s = bsa->GetSocket ();
          Ptr<TcpSocketBase> tcpSocket = DynamicCast<TcpSocketBase> (s);
          if (tcpSocket)
            return tcpSocket;
        }
    }
  // Try all sockets
  for (uint32_t i = 0; i < node->GetNDevices (); ++i)
    {
      Ptr<NetDevice> dev = node->GetDevice (i);
      Ptr<PointToPointNetDevice> pp = DynamicCast<PointToPointNetDevice>(dev);
      if (pp)
        {
          Ptr<Queue<Packet>> q = pp->GetQueue();
        }
    }
  return 0;
}

void CwndTrace (uint32_t oldCwnd, uint32_t newCwnd)
{
  *cwndStream->GetStream () << Simulator::Now ().GetSeconds () << " " << newCwnd << std::endl;
}

void ThroughputTrace (Ptr<PacketSink> sink)
{
  Time now = Simulator::Now ();
  double throughput = (sink->GetTotalRx () - lastTotalRx) * 8.0 / 1e6 / (now.GetSeconds () - lastRxTime.GetSeconds ());
  *thrStream->GetStream () << now.GetSeconds () << " " << throughput << std::endl;
  lastTotalRx = sink->GetTotalRx ();
  lastRxTime = now;
  Simulator::Schedule (Seconds (0.5), &ThroughputTrace, sink);
}

void QueueTrace (Ptr<QueueDisc> queue)
{
  uint32_t qSize = queue->GetCurrentSize ().GetValue ();
  *queueStream->GetStream () << Simulator::Now ().GetSeconds () << " " << qSize << std::endl;
  Simulator::Schedule (MilliSeconds(10), &QueueTrace, queue);
}

void EnterProbeRttPhase ()
{
  if (!g_socket)
    return;

  Ptr<TcpBbr> bbr = DynamicCast<TcpBbr> (g_socket);
  if (bbr)
    {
      bbr->EnterProbeRttPhase ();
      bbr->SetCwndDuringProbeRtt (4 * bbr->GetSegmentSize ());
      // Schedule exit PROBE_RTT phase after default BBR ProbeRTT duration
      Simulator::Schedule (MilliSeconds (200), &TcpBbr::ExitProbeRttPhase, bbr);
    }
  Simulator::Schedule (Seconds (10), &EnterProbeRttPhase);
}

int
main (int argc, char *argv[])
{
  Time::SetResolution (Time::NS);
  Config::SetDefault ("ns3::TcpL4Protocol::SocketType", TypeIdValue (TcpBbr::GetTypeId ()));
  Config::SetDefault ("ns3::TcpBbr::ProbeRttDuration", TimeValue (MilliSeconds (200)));
  Config::SetDefault ("ns3::TcpBbr::MinPipeCwnd", UintegerValue (4));

  NodeContainer nodes;
  nodes.Create (4);

  Ptr<Node> sender = nodes.Get (0);
  Ptr<Node> r1 = nodes.Get (1);
  Ptr<Node> r2 = nodes.Get (2);
  Ptr<Node> receiver = nodes.Get (3);

  NodeContainer n0n1 (sender, r1);
  NodeContainer n1n2 (r1, r2);
  NodeContainer n2n3 (r2, receiver);

  PointToPointHelper p2p1, p2p2, p2p_bottleneck;
  p2p1.SetDeviceAttribute ("DataRate", StringValue ("1000Mbps"));
  p2p1.SetChannelAttribute ("Delay", StringValue ("5ms"));

  p2p2.SetDeviceAttribute ("DataRate", StringValue ("1000Mbps"));
  p2p2.SetChannelAttribute ("Delay", StringValue ("5ms"));

  p2p_bottleneck.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  p2p_bottleneck.SetChannelAttribute ("Delay", StringValue ("10ms"));

  NetDeviceContainer d0d1 = p2p1.Install (n0n1);
  NetDeviceContainer d1d2 = p2p_bottleneck.Install (n1n2);
  NetDeviceContainer d2d3 = p2p2.Install (n2n3);

  TrafficControlHelper tch;
  uint16_t handle = tch.SetRootQueueDisc ("ns3::FqCoDelQueueDisc");
  QueueDiscContainer qDiscs = tch.Install (d1d2);

  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper address1, address2, address3;
  address1.SetBase ("10.0.1.0", "255.255.255.0");
  address2.SetBase ("10.0.2.0", "255.255.255.0");
  address3.SetBase ("10.0.3.0", "255.255.255.0");

  Ipv4InterfaceContainer i0i1 = address1.Assign (d0d1);
  Ipv4InterfaceContainer i1i2 = address2.Assign (d1d2);
  Ipv4InterfaceContainer i2i3 = address3.Assign (d2d3);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  uint16_t sinkPort = 50000;
  Address sinkAddress (InetSocketAddress (i2i3.GetAddress (1), sinkPort));
  PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), sinkPort));
  ApplicationContainer sinkApp = sinkHelper.Install (receiver);
  sinkApp.Start (Seconds (0.0));
  sinkApp.Stop (Seconds (100.0));

  BulkSendHelper source ("ns3::TcpSocketFactory", sinkAddress);
  source.SetAttribute ("MaxBytes", UintegerValue (0));
  ApplicationContainer sourceApp = source.Install (sender);
  sourceApp.Start (Seconds (0.1));
  sourceApp.Stop (Seconds (100.0));

  p2p1.EnablePcapAll ("p2p_sender_r1");
  p2p_bottleneck.EnablePcapAll ("p2p_r1_r2");
  p2p2.EnablePcapAll ("p2p_r2_receiver");

  Ptr<PacketSink> sink = DynamicCast<PacketSink> (sinkApp.Get (0));

  AsciiTraceHelper ascii;
  cwndStream = ascii.CreateFileStream ("cwnd.txt");
  thrStream = ascii.CreateFileStream ("throughput.txt");
  queueStream = ascii.CreateFileStream ("queue.txt");

  Simulator::Schedule (Seconds(0.0), &ThroughputTrace, sink);
  Simulator::Schedule (MilliSeconds(10.0), &QueueTrace, qDiscs.Get (0));

  Simulator::Schedule (Seconds (0.15), [&] {
    Ptr<TcpSocketBase> socket = GetTcpSocketFromNode (sender);
    if (!socket)
      {
        Ptr<Socket> s = Socket::CreateSocket (sender, TcpBbr::GetTypeId ());
        socket = DynamicCast<TcpSocketBase> (s);
      }
    g_socket = socket;
    if (g_socket)
      {
        g_socket->TraceConnectWithoutContext ("CongestionWindow", MakeCallback (&CwndTrace));
      }
    Simulator::Schedule (Seconds (10.0), &EnterProbeRttPhase);
  });

  Simulator::Stop (Seconds (100.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}