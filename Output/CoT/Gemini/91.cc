#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/socket.h"
#include "ns3/udp-socket-factory.h"
#include "ns3/packet.h"
#include "ns3/uinteger.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("MultiInterfaceRouting");

static void
ReceivePacket (Ptr<Socket> socket)
{
  Address localAddress = socket->GetLocalAddress();
  Ipv4Address ipv4LocalAddress = InetSocketAddress::ConvertFrom(localAddress).GetIpv4();

  while (socket->Recv ())
    {
      NS_LOG_UNCOND ("Received one packet on interface " << ipv4LocalAddress);
    }
}

static void
SentPacket (Ptr<Socket> socket, uint32_t pktSize)
{
  Address localAddress = socket->GetLocalAddress();
  Ipv4Address ipv4LocalAddress = InetSocketAddress::ConvertFrom(localAddress).GetIpv4();

  NS_LOG_UNCOND ("Sent one packet from interface " << ipv4LocalAddress << " size " << pktSize);
}


int
main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);
  LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
  LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);
  LogComponentEnable ("MultiInterfaceRouting", LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create (4);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer devices01 = pointToPoint.Install (nodes.Get (0), nodes.Get (1));
  NetDeviceContainer devices02 = pointToPoint.Install (nodes.Get (0), nodes.Get (2));
  NetDeviceContainer devices13 = pointToPoint.Install (nodes.Get (1), nodes.Get (3));
  NetDeviceContainer devices23 = pointToPoint.Install (nodes.Get (2), nodes.Get (3));

  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces01 = address.Assign (devices01);

  address.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces02 = address.Assign (devices02);

  address.SetBase ("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces13 = address.Assign (devices13);

  address.SetBase ("10.1.4.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces23 = address.Assign (devices23);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  Ptr<Ipv4StaticRouting> staticRouting1 = Ipv4RoutingHelper::GetRouting <Ipv4StaticRouting> (nodes.Get (1)->GetObject<Ipv4> ()->GetRoutingProtocol ());
  staticRouting1->AddHostRouteToNetwork (Ipv4Address ("10.1.4.0"), Ipv4Mask ("255.255.255.0"), Ipv4Address ("10.1.3.2"), 1);

  Ptr<Ipv4StaticRouting> staticRouting2 = Ipv4RoutingHelper::GetRouting <Ipv4StaticRouting> (nodes.Get (2)->GetObject<Ipv4> ()->GetRoutingProtocol ());
  staticRouting2->AddHostRouteToNetwork (Ipv4Address ("10.1.4.0"), Ipv4Mask ("255.255.255.0"), Ipv4Address ("10.1.4.2"), 2);


  uint16_t port = 9;

  Address sinkLocalAddress (InetSocketAddress (Ipv4Address::GetAny (), port));
  Ptr<Socket> sinkSocket1 = Socket::CreateSocket (nodes.Get (3), UdpSocketFactory::GetTypeId ());
  sinkSocket1->Bind (sinkLocalAddress);
  sinkSocket1->SetRecvCallback (MakeCallback (&ReceivePacket));

  Ptr<Socket> sinkSocket2 = Socket::CreateSocket (nodes.Get (3), UdpSocketFactory::GetTypeId ());
  sinkSocket2->Bind (InetSocketAddress (Ipv4Address::GetAny (), port+1));
  sinkSocket2->SetRecvCallback (MakeCallback (&ReceivePacket));

  Ptr<Socket> sourceSocket1 = Socket::CreateSocket (nodes.Get (0), UdpSocketFactory::GetTypeId ());
  sourceSocket1->Bind (InetSocketAddress (interfaces01.GetAddress (0), 0));
  sourceSocket1->SetAllowBroadcast (true);

  Ptr<Socket> sourceSocket2 = Socket::CreateSocket (nodes.Get (0), UdpSocketFactory::GetTypeId ());
  sourceSocket2->Bind (InetSocketAddress (interfaces02.GetAddress (0), 0));
  sourceSocket2->SetAllowBroadcast (true);

  InetSocketAddress remoteAddress1 (interfaces23.GetAddress (1), port);
  InetSocketAddress remoteAddress2 (interfaces23.GetAddress (1), port+1);

  sourceSocket1->Connect (remoteAddress1);
  sourceSocket2->Connect (remoteAddress2);

  sourceSocket1->SetSendCallback(MakeCallback (&SentPacket));
  sourceSocket2->SetSendCallback(MakeCallback (&SentPacket));


  Simulator::ScheduleWithContext (sourceSocket1->GetNode ()->GetId (), Seconds (1.0), [&](){
    Ptr<Packet> pkt = Create<Packet> (1024);
    sourceSocket1->Send (pkt);
  });

  Simulator::ScheduleWithContext (sourceSocket2->GetNode ()->GetId (), Seconds (1.1), [&](){
    Ptr<Packet> pkt = Create<Packet> (1024);
    sourceSocket2->Send (pkt);
  });


  Simulator::Stop (Seconds (10.0));

  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}