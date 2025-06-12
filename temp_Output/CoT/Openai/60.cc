#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/packet-sink.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/trace-helper.h"
#include "ns3/traffic-control-module.h"
#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("TcpLargeTransfer");

static void
CwndChange (Ptr<OutputStreamWrapper> stream, uint32_t oldCwnd, uint32_t newCwnd)
{
  *stream->GetStream () << Simulator::Now ().GetSeconds () << " " << newCwnd << std::endl;
}

static void
RxTrace (Ptr<OutputStreamWrapper> stream, Ptr<const Packet> packet, const Address &)
{
  *stream->GetStream () << Simulator::Now ().GetSeconds () << " RX " << packet->GetSize () << std::endl;
}

int 
main (int argc, char *argv[])
{
  // Configure ns-3
  CommandLine cmd;
  cmd.Parse (argc, argv);

  // Create nodes
  NodeContainer nodes;
  nodes.Create (3);
  Names::Add ("n0", nodes.Get (0));
  Names::Add ("n1", nodes.Get (1));
  Names::Add ("n2", nodes.Get (2));

  // Point-to-point helpers
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("10ms"));

  // First link: n0 <-> n1
  NetDeviceContainer d01 = p2p.Install (nodes.Get (0), nodes.Get (1));

  // Second link: n1 <-> n2
  NetDeviceContainer d12 = p2p.Install (nodes.Get (1), nodes.Get (2));

  // Install Internet stack
  InternetStackHelper stack;
  stack.InstallAll ();

  // Assign IP addresses
  Ipv4AddressHelper ip;
  ip.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i01 = ip.Assign (d01);

  ip.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer i12 = ip.Assign (d12);

  // Enable PCAP
  p2p.EnablePcap ("tcp-large-transfer-0-0", d01.Get (0), true, true);
  p2p.EnablePcap ("tcp-large-transfer-1-0", d01.Get (1), true, true);
  p2p.EnablePcap ("tcp-large-transfer-1-1", d12.Get (0), true, true);
  p2p.EnablePcap ("tcp-large-transfer-2-1", d12.Get (1), true, true);

  // Assign routing
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // Install PacketSinkApplication on n2
  uint16_t sinkPort = 8080;
  Address sinkAddress (InetSocketAddress (i12.GetAddress (1), sinkPort));
  PacketSinkHelper packetSinkHelper ("ns3::TcpSocketFactory", sinkAddress);
  ApplicationContainer sinkApp = packetSinkHelper.Install (nodes.Get (2));
  sinkApp.Start (Seconds (0.0));
  sinkApp.Stop (Seconds (20.0));

  // Trace packet reception to file
  AsciiTraceHelper ascii;
  Ptr<OutputStreamWrapper> stream = ascii.CreateFileStream ("tcp-large-transfer.tr");
  Ptr<PacketSink> sink = DynamicCast<PacketSink> (sinkApp.Get (0));
  sink->TraceConnectWithoutContext ("Rx", MakeBoundCallback (&RxTrace, stream));

  // Implement sender using socket on n0
  Ptr<Node> srcNode = nodes.Get (0);
  Ptr<Ipv4> ipv4 = srcNode->GetObject<Ipv4> ();
  Ipv4Address srcAddr = ipv4->GetAddress (1,0).GetLocal ();

  Ptr<Socket> srcSocket = Socket::CreateSocket (srcNode, TcpSocketFactory::GetTypeId ());
  srcSocket->Connect (InetSocketAddress (i12.GetAddress (1), sinkPort));

  // Trace congestion window
  Ptr<OutputStreamWrapper> cwndStream = ascii.CreateFileStream ("cwnd-trace.tr");
  srcSocket->TraceConnectWithoutContext ("CongestionWindow", MakeBoundCallback (&CwndChange, cwndStream));

  // Send data
  uint32_t totalBytes = 2000000;
  static uint32_t bytesSent = 0;
  static uint32_t maxBytes = totalBytes;
  uint32_t segmentSize = 1000;

  static EventId sendEvent;

  // Sending function
  struct SendHelper
  {
    static void SendData (Ptr<Socket> sock, uint32_t pktSize, Ptr<Ipv4> ipv4, Ptr<Ipv4> destIpv4, uint16_t port)
    {
      while (bytesSent < maxBytes && sock->GetTxAvailable () >= pktSize)
        {
          uint32_t toSend = std::min (pktSize, maxBytes - bytesSent);
          Ptr<Packet> packet = Create<Packet> (toSend);
          int sent = sock->Send (packet);
          if (sent > 0)
            {
              bytesSent += sent;
            }
          else
            {
              break;
            }
        }
      if (bytesSent < maxBytes)
        {
          sendEvent = Simulator::Schedule (MilliSeconds (1), &SendHelper::SendData, sock, pktSize, ipv4, destIpv4, port);
        }
    }
  };

  // Start sending at 1s
  Simulator::Schedule (Seconds (1.0), &SendHelper::SendData, srcSocket, segmentSize, ipv4, nullptr, sinkPort);

  Simulator::Stop (Seconds (20.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}