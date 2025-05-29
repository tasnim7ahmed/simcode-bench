#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/socket.h"
#include "ns3/udp-socket-factory.h"
#include "ns3/packet.h"
#include "ns3/uinteger.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("UdpRelay");

static void
ReceivePacket (Ptr<Socket> socket)
{
  Address from;
  Ptr<Packet> packet;
  while ((packet = socket->RecvFrom (from)))
    {
      if (packet->GetSize () > 0)
        {
          std::cout << Simulator::Now ().AsTimeStep () << " Received " << packet->GetSize () << " bytes from " << from << std::endl;
        }
    }
}

int
main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);
  LogComponent::SetFilter ( "UdpRelay", LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create (3); // Client, Relay, Server

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer devices1, devices2;
  devices1 = pointToPoint.Install (nodes.Get (0), nodes.Get (1));
  devices2 = pointToPoint.Install (nodes.Get (1), nodes.Get (2));

  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces1 = address.Assign (devices1);

  address.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces2 = address.Assign (devices2);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  uint16_t port = 9;

  Ptr<Socket> recvSocket = Socket::CreateSocket (nodes.Get (2), UdpSocketFactory::GetTypeId ());
  InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), port);
  recvSocket->Bind (local);
  recvSocket->SetRecvCallback (MakeCallback (&ReceivePacket));

  TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
  Ptr<Socket> sourceSocket = Socket::CreateSocket (nodes.Get (0), tid);
  InetSocketAddress remote = InetSocketAddress (interfaces1.GetAddress (1), port);
  sourceSocket->Connect (remote);

  Simulator::ScheduleWithContext (sourceSocket->GetNode ()->GetId (), Seconds (1.0), [sourceSocket, interfaces2, port]() {
    Ptr<Packet> packet = Create<Packet> ((uint8_t*)"Hello, World!", 13);
    InetSocketAddress relayAddress = InetSocketAddress (interfaces2.GetAddress (1), port);
    sourceSocket->SendTo (packet, 0, relayAddress);
  });

  Simulator::ScheduleWithContext (nodes.Get(1)->GetId(), Seconds (1.1), [interfaces1, interfaces2, port](){
    TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
    Ptr<Socket> relayRecvSocket = Socket::CreateSocket (NodeList::GetNode (1), tid);
    InetSocketAddress relayLocal = InetSocketAddress (interfaces1.GetAddress (1), port);
    relayRecvSocket->Bind (relayLocal);

    relayRecvSocket->SetRecvCallback ([interfaces2, port] (Ptr<Socket> socket) {
      Address from;
      Ptr<Packet> packet;
      while ((packet = socket->RecvFrom (from))) {
          TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
          Ptr<Socket> relaySendSocket = Socket::CreateSocket (NodeList::GetNode (1), tid);
          InetSocketAddress serverAddress = InetSocketAddress (interfaces2.GetAddress (1), port);
          relaySendSocket->Connect (serverAddress);
          relaySendSocket->Send (packet);
      }
    });
  });

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}