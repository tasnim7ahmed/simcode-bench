#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/udp-socket-factory.h"
#include "ns3/packet.h"

using namespace ns3;

static void
ReceivePacket (Ptr<Socket> socket)
{
  Address localAddress;
  socket->GetSockName(localAddress);
  while (socket->Recv ())
    {
      NS_LOG_UNCOND ("Received one packet at " << localAddress);
    }
}

static void
SendPacket (Ptr<Socket> socket, Address destAddress, uint32_t packetSize)
{
  Ptr<Packet> packet = Create<Packet> (packetSize);
  socket->SendTo (packet, 0, destAddress);
}

int main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);

  NodeContainer nodes;
  nodes.Create (4); // Source, Router1, Router2, Destination

  NodeContainer sourceRouter1 = NodeContainer (nodes.Get (0), nodes.Get (1));
  NodeContainer sourceRouter2 = NodeContainer (nodes.Get (0), nodes.Get (2));
  NodeContainer router1Dest = NodeContainer (nodes.Get (1), nodes.Get (3));
  NodeContainer router2Dest = NodeContainer (nodes.Get (2), nodes.Get (3));

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer sourceRouter1Devices = pointToPoint.Install (sourceRouter1);
  NetDeviceContainer sourceRouter2Devices = pointToPoint.Install (sourceRouter2);
  NetDeviceContainer router1DestDevices = pointToPoint.Install (router1Dest);
  NetDeviceContainer router2DestDevices = pointToPoint.Install (router2Dest);

  InternetStackHelper internet;
  internet.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer sourceRouter1Interfaces = address.Assign (sourceRouter1Devices);

  address.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer sourceRouter2Interfaces = address.Assign (sourceRouter2Devices);

  address.SetBase ("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer router1DestInterfaces = address.Assign (router1DestDevices);

  address.SetBase ("10.1.4.0", "255.255.255.0");
  Ipv4InterfaceContainer router2DestInterfaces = address.Assign (router2DestDevices);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  Ipv4StaticRoutingHelper staticRouting;
  Ptr<Ipv4StaticRouting> router1Routing = staticRouting.GetStaticRouting (nodes.Get (1)->GetObject<Ipv4> ());
  router1Routing->AddHostRouteTo (Ipv4Address ("10.1.4.2"), Ipv4Address ("10.1.3.2"), 1); // Metric 1

  Ptr<Ipv4StaticRouting> router2Routing = staticRouting.GetStaticRouting (nodes.Get (2)->GetObject<Ipv4> ());
  router2Routing->AddHostRouteTo (Ipv4Address ("10.1.3.2"), Ipv4Address ("10.1.4.2"), 2); // Metric 2

  uint16_t port = 9;
  InetSocketAddress destinationAddress (router1DestInterfaces.GetAddress (1), port);

  Ptr<Socket> socket1 = Socket::CreateSocket (nodes.Get (0), UdpSocketFactory::GetTypeId ());
  socket1->BindToNetDevice (sourceRouter1Devices.Get (0));
  socket1->Bind (InetSocketAddress (Ipv4Address::GetAny (), 0));
  socket1->SetRecvCallback (MakeCallback (&ReceivePacket));
  socket1->SetAllowBroadcast(true);

  Ptr<Socket> socket2 = Socket::CreateSocket (nodes.Get (0), UdpSocketFactory::GetTypeId ());
  socket2->BindToNetDevice (sourceRouter2Devices.Get (0));
  socket2->Bind (InetSocketAddress (Ipv4Address::GetAny (), 0));
  socket2->SetRecvCallback (MakeCallback (&ReceivePacket));
  socket2->SetAllowBroadcast(true);

  Ptr<Socket> recvSocket = Socket::CreateSocket (nodes.Get (3), UdpSocketFactory::GetTypeId ());
  InetSocketAddress localAddress (Ipv4Address::GetAny (), port);
  recvSocket->Bind (localAddress);
  recvSocket->SetRecvCallback (MakeCallback (&ReceivePacket));
  recvSocket->SetAllowBroadcast(true);

  Simulator::Schedule (Seconds (1.0), &SendPacket, socket1, destinationAddress, 1024);
  Simulator::Schedule (Seconds (2.0), &SendPacket, socket2, destinationAddress, 1024);
  Simulator::Schedule (Seconds (3.0), &SendPacket, socket1, destinationAddress, 1024);

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}