#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/log.h"
#include "ns3/socket.h"
#include "ns3/udp-socket-factory.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("UdpRelay");

static void
ReceivePacket (Ptr<Socket> socket)
{
  Address from;
  Ptr<Packet> packet = socket->RecvFrom (from);
  if (packet->GetSize () > 0)
    {
      NS_LOG_UNCOND ("Received " << packet->GetSize () << " bytes from " << from);
    }
}

static void
ForwardPacket (Ptr<Socket> socket, Ipv4Address destinationAddress, uint16_t destinationPort)
{
  Address from;
  Ptr<Packet> packet = socket->RecvFrom (from);
  if (packet->GetSize () > 0)
    {
      NS_LOG_UNCOND ("Relay forwarding packet of size " << packet->GetSize() << " bytes");
      Ptr<Socket> outgoingSocket = Socket::CreateSocket (socket->GetNode (), UdpSocketFactory::GetTypeId ());
      outgoingSocket->Connect (InetSocketAddress {destinationAddress, destinationPort});
      outgoingSocket->Send (packet);
    }
}


int
main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);
  LogComponentEnable ("UdpRelay", LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create (3);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer devices1 = pointToPoint.Install (nodes.Get (0), nodes.Get (1));
  NetDeviceContainer devices2 = pointToPoint.Install (nodes.Get (1), nodes.Get (2));

  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces1 = address.Assign (devices1);

  address.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces2 = address.Assign (devices2);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  uint16_t port = 9;
  uint16_t relayPort = 10;

  // Server (Node 2)
  Ptr<Socket> recvSink = Socket::CreateSocket (nodes.Get (2), UdpSocketFactory::GetTypeId ());
  InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), port);
  recvSink->Bind (local);
  recvSink->SetRecvCallback (MakeCallback (&ReceivePacket));

  // Relay (Node 1)
  Ptr<Socket> relayRecvSocket = Socket::CreateSocket (nodes.Get (1), UdpSocketFactory::GetTypeId ());
  InetSocketAddress relayLocal = InetSocketAddress (Ipv4Address::GetAny (), relayPort);
  relayRecvSocket->Bind (relayLocal);

  Ipv4Address serverAddress = interfaces2.GetAddress (1);
  relayRecvSocket->SetRecvCallback (MakeCallback (&ForwardPacket, serverAddress, port));


  // Client (Node 0)
  Ptr<Socket> clientSocket = Socket::CreateSocket (nodes.Get (0), UdpSocketFactory::GetTypeId ());
  InetSocketAddress remote = InetSocketAddress (interfaces1.GetAddress (1), relayPort);
  clientSocket->Connect (remote);

  Ptr<Packet> packet = Create<Packet> ((uint8_t*) "Hello World", 12);
  clientSocket->Send (packet);

  Simulator::Stop (Seconds (1.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}