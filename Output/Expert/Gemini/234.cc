#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/socket.h"
#include "ns3/udp-socket-factory.h"

using namespace ns3;

static void
ReceivePacket (Ptr<Socket> socket)
{
  Address from;
  Ptr<Packet> packet = socket->RecvFrom (from);
  if (packet->GetSize() > 0)
  {
    std::cout << Simulator::Now ().AsTime (Time::S) << "s: Received " << packet->GetSize () << " bytes from " << from << std::endl;
  }
}

static void
ClientSend (Ptr<Socket> socket, Address dstAddress, uint32_t packetSize, uint32_t numPackets)
{
  for (uint32_t i = 0; i < numPackets; ++i)
  {
    Ptr<Packet> packet = Create<Packet> (packetSize);
    socket->SendTo (packet, 0, dstAddress);
  }
}

int main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);
  LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
  LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);

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

  Address serverAddress (InetSocketAddress (interfaces2.GetAddress (1), port));

  TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
  Ptr<Socket> recvSocket = Socket::CreateSocket (nodes.Get (2), tid);
  InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), port);
  recvSocket->Bind (local);
  recvSocket->SetRecvCallback (MakeCallback (&ReceivePacket));

  Ptr<Socket> clientSocket = Socket::CreateSocket (nodes.Get (0), tid);
  clientSocket->Bind ();

  Simulator::ScheduleWithContext (clientSocket->GetNode ()->GetId (), Seconds (1.0), &ClientSend, clientSocket, serverAddress, 1024, 10);

  Packet::EnableDropping ();

  Simulator::Stop (Seconds (10.0));

  Simulator::Run ();

  Simulator::Destroy ();

  return 0;
}