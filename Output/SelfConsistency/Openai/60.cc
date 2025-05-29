#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("TcpLargeTransferExample");

void
CwndChangeCallback (Ptr<OutputStreamWrapper> stream, uint32_t oldCwnd, uint32_t newCwnd)
{
  *stream->GetStream ()
    << Simulator::Now ().GetSeconds () << " " << newCwnd << std::endl;
}

int
main (int argc, char *argv[])
{
  // Enable logging
  // LogComponentEnable ("TcpLargeTransferExample", LOG_LEVEL_INFO);

  // Set simulation parameters
  uint32_t maxBytes = 2000000;

  // Create nodes
  NodeContainer nodes;
  nodes.Create (3);

  // Set up point-to-point links
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("10ms"));

  // n0 <-> n1
  NetDeviceContainer d01 = p2p.Install (nodes.Get (0), nodes.Get (1));
  // n1 <-> n2
  NetDeviceContainer d12 = p2p.Install (nodes.Get (1), nodes.Get (2));

  // Install Internet stack
  InternetStackHelper internet;
  internet.Install (nodes);

  // Assign IP addresses
  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i01 = ipv4.Assign (d01);

  ipv4.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer i12 = ipv4.Assign (d12);

  // Enable pcap tracing with custom filenames
  p2p.EnablePcap ("tcp-large-transfer-0-1", d01.Get (0), true, true);
  p2p.EnablePcap ("tcp-large-transfer-1-1", d01.Get (1), true, true);
  p2p.EnablePcap ("tcp-large-transfer-1-2", d12.Get (0), true, true);
  p2p.EnablePcap ("tcp-large-transfer-2-2", d12.Get (1), true, true);

  // Enable ASCII tracing
  AsciiTraceHelper ascii;
  Ptr<OutputStreamWrapper> stream = ascii.CreateFileStream ("tcp-large-transfer.tr");
  p2p.EnableAsciiAll (stream);

  // Install applications (PacketSink on n2)
  uint16_t sinkPort = 8080;
  Address sinkAddress (InetSocketAddress (i12.GetAddress (1), sinkPort));
  PacketSinkHelper packetSinkHelper ("ns3::TcpSocketFactory", sinkAddress);
  ApplicationContainer sinkApp = packetSinkHelper.Install (nodes.Get (2));
  sinkApp.Start (Seconds (0.0));
  sinkApp.Stop (Seconds (30.0));

  // Implement TCP sender using a socket on n0
  Ptr<Node> srcNode = nodes.Get (0);
  Ptr<Socket> srcSocket = Socket::CreateSocket (srcNode, TcpSocketFactory::GetTypeId ());

  // Callback for congestion window tracing
  Ptr<OutputStreamWrapper> cwndStream = ascii.CreateFileStream ("cwnd-trace.txt");
  srcSocket->TraceConnectWithoutContext ("CongestionWindow",
                                         MakeBoundCallback (&CwndChangeCallback, cwndStream));

  // Connect socket to sink
  srcSocket->Connect (InetSocketAddress (i12.GetAddress (1), sinkPort));

  // Send 2,000,000 bytes from sender using Send() loop
  static uint32_t totalBytesSent = 0;
  static uint32_t dataSize = 1000;

  // Lambda to drive data sending
  std::function<void (Ptr<Socket>)> sendData;
  EventId sendEvent;

  sendData = [&] (Ptr<Socket> socket)
    {
      while (socket->GetTxAvailable () >= dataSize && totalBytesSent < maxBytes)
        {
          uint32_t bytesToSend = std::min (dataSize, maxBytes - totalBytesSent);
          Ptr<Packet> packet = Create<Packet> (bytesToSend);
          int actual = socket->Send (packet);
          if (actual > 0)
            {
              totalBytesSent += actual;
            }
          if (totalBytesSent >= maxBytes)
            {
              break;
            }
        }
      if (totalBytesSent < maxBytes)
        {
          sendEvent = Simulator::Schedule (MilliSeconds (1), sendData, socket);
        }
    };

  // Callback for handling "Send" opportunity
  srcSocket->SetSendCallback (
      MakeCallback ([&] (Ptr<Socket> socket, uint32_t available) {
        if (totalBytesSent < maxBytes)
          {
            Simulator::ScheduleNow (sendData, socket);
          }
      }));

  // Start simulation
  Simulator::Schedule (Seconds (1.0), sendData, srcSocket);

  // Track received packets at sink
  sinkApp.Get (0)->TraceConnectWithoutContext (
      "Rx", MakeCallback (
          [](Ptr<const Packet> packet, const Address &from) {
            NS_LOG_INFO ("Packet received at sink: " << packet->GetSize () << " bytes at "
                                                     << Simulator::Now ().GetSeconds () << "s");
          }));

  Simulator::Stop (Seconds (30.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}