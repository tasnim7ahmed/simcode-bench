#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/tcp-socket-factory.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("TcpSocketSimulation");

void
RxCallback (Ptr<Socket> socket)
{
  Address from;
  Ptr<Packet> packet;
  while ((packet = socket->RecvFrom (from)))
    {
      NS_LOG_INFO ("At " << Simulator::Now ().GetSeconds ()
                         << "s node " << socket->GetNode ()->GetId ()
                         << " received " << packet->GetSize () << " bytes");
    }
}

void
SendPacket (Ptr<Socket> socket, Ipv4Address dstAddr, uint16_t dstPort, uint32_t pktSize, uint32_t pktCount, Time pktInterval)
{
  if (pktCount == 0)
    {
      socket->Close ();
      return;
    }
  Ptr<Packet> pkt = Create<Packet> (pktSize);
  socket->SendTo (pkt, 0, InetSocketAddress (dstAddr, dstPort));
  NS_LOG_INFO ("At " << Simulator::Now ().GetSeconds ()
                     << "s node " << socket->GetNode ()->GetId ()
                     << " sent " << pktSize << " bytes");
  Simulator::Schedule (pktInterval, &SendPacket, socket, dstAddr, dstPort, pktSize, pktCount - 1, pktInterval);
}

int
main (int argc, char *argv[])
{
  LogComponentEnable ("TcpSocketSimulation", LOG_LEVEL_INFO);
  NodeContainer nodes;
  nodes.Create (2);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer devices = p2p.Install (nodes);

  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = ipv4.Assign (devices);

  uint16_t tcpPort = 50000;

  // Server socket (Node 1)
  TypeId tcpTid = TypeId::LookupByName ("ns3::TcpSocketFactory");
  Ptr<Socket> recvSocket = Socket::CreateSocket (nodes.Get (1), tcpTid);
  InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), tcpPort);
  recvSocket->Bind (local);
  recvSocket->Listen ();
  recvSocket->SetRecvCallback (MakeCallback (&RxCallback));

  // Client socket (Node 0)
  Ptr<Socket> sendSocket = Socket::CreateSocket (nodes.Get (0), tcpTid);
  sendSocket->Bind ();
  Simulator::ScheduleWithContext (sendSocket->GetNode ()->GetId (), Seconds (1.0),
      &SendPacket, sendSocket, interfaces.GetAddress (1), tcpPort,
      1024, // bytes per pkt
      10,   // pkt count
      MilliSeconds (200) // interval
      );

  Simulator::Stop (Seconds (5.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}