#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/packet-sink.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/trace-helper.h"

using namespace ns3;

void
CwndTracer (std::string context, uint32_t oldCwnd, uint32_t newCwnd)
{
  static std::ofstream out ("cwnd-trace.txt", std::ios_base::app);
  out << Simulator::Now ().GetSeconds () << " " << newCwnd << std::endl;
}

static void SendData (Ptr<Socket> socket, uint32_t totalBytes, Ptr<UniformRandomVariable> urv, uint32_t *sent)
{
  const uint32_t maxPacketSize = 1000;
  while (*sent < totalBytes)
    {
      uint32_t toSend = std::min (maxPacketSize, totalBytes - *sent);
      Ptr<Packet> p = Create<Packet> (toSend);
      int actual = socket->Send (p);
      if (actual > 0)
        {
          *sent += actual;
        }
      if (actual < (int)toSend)
        {
          // Blocked, schedule new attempt later
          Simulator::Schedule (Seconds (0.001 * urv->GetValue (0.7,1.3)),
                               &SendData, socket, totalBytes, urv, sent);
          return;
        }
      if (*sent == totalBytes)
        break;
    }
}

int
main (int argc, char *argv[])
{
  // Nodes
  NodeContainer nodes;
  nodes.Create (3);

  // Links
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("10ms"));

  NetDeviceContainer d0d1 = p2p.Install (nodes.Get (0), nodes.Get (1));
  NetDeviceContainer d1d2 = p2p.Install (nodes.Get (1), nodes.Get (2));

  // Tracing: Queue
  AsciiTraceHelper ascii;
  Ptr<OutputStreamWrapper> stream = ascii.CreateFileStream ("tcp-large-transfer.tr");
  p2p.EnableAsciiAll (stream);
  p2p.EnablePcap ("tcp-large-transfer-0-1", d0d1, true, true);
  p2p.EnablePcap ("tcp-large-transfer-1-2", d1d2, true, true);

  // Internet Stack
  InternetStackHelper stack;
  stack.Install (nodes);

  // IP addresses
  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i0i1 = ipv4.Assign (d0d1);

  ipv4.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer i1i2 = ipv4.Assign (d1d2);

  // Routing
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // PacketSink on n2
  uint16_t sinkPort = 50000;
  Address sinkAddress (InetSocketAddress (i1i2.GetAddress (1), sinkPort));
  PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory", sinkAddress);
  ApplicationContainer sinkApp = sinkHelper.Install (nodes.Get (2));
  sinkApp.Start (Seconds (0.0));
  sinkApp.Stop (Seconds (20.0));

  // Sender socket on n0
  Ptr<Socket> sock = Socket::CreateSocket (nodes.Get (0), TcpSocketFactory::GetTypeId ());

  // Congestion window trace
  sock->TraceConnect ("CongestionWindow", std::string (), MakeCallback (&CwndTracer));

  // Connect
  sock->Connect (InetSocketAddress (i1i2.GetAddress (1), sinkPort));

  // Schedule send
  const uint32_t totalBytes = 2000000;
  Ptr<UniformRandomVariable> urv = CreateObject<UniformRandomVariable> ();
  uint32_t *sent = new uint32_t (0);

  Simulator::ScheduleWithContext (sock->GetNode ()->GetId (),
                                  Seconds (1.0),
                                  &SendData, sock, totalBytes, urv, sent);

  // Trace packet receptions at sink
  sinkApp.Get (0)->TraceConnectWithoutContext ("Rx", MakeBoundCallback (
    [] (Ptr<const Packet> pkt, const Address &) {
      static std::ofstream out ("rx-packets.txt", std::ios_base::app);
      out << Simulator::Now ().GetSeconds () << " " << pkt->GetSize () << std::endl;
    }));

  Simulator::Stop (Seconds (20.0));
  Simulator::Run ();
  Simulator::Destroy ();
  delete sent;
  return 0;
}