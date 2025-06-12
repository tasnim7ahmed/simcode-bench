/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("MultiInterfaceStaticRoutingExample");

void
ReceivePacketAtDestination (Ptr<Socket> socket)
{
  Address from;
  while (Ptr<Packet> packet = socket->RecvFrom (from))
    {
      InetSocketAddress address = InetSocketAddress::ConvertFrom (from);
      NS_LOG_INFO ("[DEST] Received packet of " << packet->GetSize ()
                    << " bytes from " << address.GetIpv4 ());
    }
}

void
ReceivePacketAtSource (Ptr<Socket> socket)
{
  Address from;
  while (Ptr<Packet> packet = socket->RecvFrom (from))
    {
      InetSocketAddress address = InetSocketAddress::ConvertFrom (from);
      NS_LOG_INFO ("[SRC] Source received packet of " << packet->GetSize ()
                    << " bytes from " << address.GetIpv4 ());
    }
}

void
SendPacket (Ptr<Socket> socket, Ipv4Address dstAddr, uint16_t dstPort, uint32_t pktSize)
{
  Ptr<Packet> packet = Create<Packet> (pktSize);
  socket->SendTo (packet, 0, InetSocketAddress (dstAddr, dstPort));
  NS_LOG_INFO ("Sent " << pktSize << " bytes to " << dstAddr);
}

int
main (int argc, char *argv[])
{
  LogComponentEnable ("MultiInterfaceStaticRoutingExample", LOG_LEVEL_INFO);

  // Create nodes: src, router1, router2, dst
  NodeContainer nodes;
  nodes.Create (4);
  Ptr<Node> src = nodes.Get (0);
  Ptr<Node> router1 = nodes.Get (1);
  Ptr<Node> router2 = nodes.Get (2);
  Ptr<Node> dst = nodes.Get (3);

  // Create two parallel paths src--r1--dst and src--r2--dst
  NodeContainer nSrcR1 (src, router1);
  NodeContainer nSrcR2 (src, router2);
  NodeContainer nR1Dst (router1, dst);
  NodeContainer nR2Dst (router2, dst);

  // Point-to-point links
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer dSrcR1 = p2p.Install (nSrcR1);
  NetDeviceContainer dSrcR2 = p2p.Install (nSrcR2);
  NetDeviceContainer dR1Dst = p2p.Install (nR1Dst);
  NetDeviceContainer dR2Dst = p2p.Install (nR2Dst);

  // Install Internet Stack
  InternetStackHelper stack;
  stack.InstallAll ();

  // Assign IP addresses
  Ipv4AddressHelper address;

  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer iSrcR1 = address.Assign (dSrcR1);

  address.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer iSrcR2 = address.Assign (dSrcR2);

  address.SetBase ("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer iR1Dst = address.Assign (dR1Dst);

  address.SetBase ("10.1.4.0", "255.255.255.0");
  Ipv4InterfaceContainer iR2Dst = address.Assign (dR2Dst);

  // Set up static routing
  Ipv4StaticRoutingHelper staticRoutingHelper;

  // Source routing (multi-interface)
  Ptr<Ipv4StaticRouting> srcStaticRouting =
    staticRoutingHelper.GetStaticRouting (src->GetObject<Ipv4> ());
  // Route 1: src->r1->dst (interface 1 of src)
  srcStaticRouting->AddNetworkRouteTo (
      Ipv4Address ("10.1.3.0"), Ipv4Mask ("255.255.255.0"),
      Ipv4Address ("10.1.1.2"), 1, 1); // next hop, interface, metric

  // Route 2: src->r2->dst (interface 2 of src), higher metric (less preferred)
  srcStaticRouting->AddNetworkRouteTo (
      Ipv4Address ("10.1.4.0"), Ipv4Mask ("255.255.255.0"),
      Ipv4Address ("10.1.2.2"), 2, 10);

  // Set up routing on Routers
  Ptr<Ipv4StaticRouting> r1StaticRouting =
    staticRoutingHelper.GetStaticRouting (router1->GetObject<Ipv4> ());
  r1StaticRouting->AddNetworkRouteTo (
      Ipv4Address ("10.1.4.0"), Ipv4Mask ("255.255.255.0"),
      Ipv4Address ("10.1.3.2"), 2); // for traffic from r1 to dst

  r1StaticRouting->AddNetworkRouteTo (
      Ipv4Address ("10.1.2.0"), Ipv4Mask ("255.255.255.0"),
      Ipv4Address ("10.1.1.1"), 1);

  Ptr<Ipv4StaticRouting> r2StaticRouting =
    staticRoutingHelper.GetStaticRouting (router2->GetObject<Ipv4> ());
  r2StaticRouting->AddNetworkRouteTo (
      Ipv4Address ("10.1.3.0"), Ipv4Mask ("255.255.255.0"),
      Ipv4Address ("10.1.4.2"), 2);

  r2StaticRouting->AddNetworkRouteTo (
      Ipv4Address ("10.1.1.0"), Ipv4Mask ("255.255.255.0"),
      Ipv4Address ("10.1.2.1"), 1);

  // Destination default route
  Ptr<Ipv4StaticRouting> dstStaticRouting =
    staticRoutingHelper.GetStaticRouting (dst->GetObject<Ipv4> ());
  dstStaticRouting->SetDefaultRoute ("10.1.3.1", 1);

  // Set up receiving socket at destination
  TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
  Ptr<Socket> dstSocket = Socket::CreateSocket (dst, tid);
  InetSocketAddress dstAddr (iR1Dst.GetAddress (1), 5555);
  dstSocket->Bind (dstAddr);
  dstSocket->SetRecvCallback (MakeCallback (&ReceivePacketAtDestination));

  // Set up source sockets: bind to each interface separately
  Ptr<Socket> srcSocket1 = Socket::CreateSocket (src, tid);
  InetSocketAddress srcAddr1 (iSrcR1.GetAddress (0), 5556);
  srcSocket1->Bind (srcAddr1);
  srcSocket1->SetRecvCallback (MakeCallback (&ReceivePacketAtSource));

  Ptr<Socket> srcSocket2 = Socket::CreateSocket (src, tid);
  InetSocketAddress srcAddr2 (iSrcR2.GetAddress (0), 5557);
  srcSocket2->Bind (srcAddr2);
  srcSocket2->SetRecvCallback (MakeCallback (&ReceivePacketAtSource));

  // Schedule packet sends - alternate between interfaces
  Simulator::Schedule (Seconds (1.0), &SendPacket, srcSocket1, iR1Dst.GetAddress (1), 5555, 512);
  Simulator::Schedule (Seconds (2.0), &SendPacket, srcSocket2, iR2Dst.GetAddress (1), 5555, 512);
  Simulator::Schedule (Seconds (3.0), &SendPacket, srcSocket1, iR1Dst.GetAddress (1), 5555, 512);
  Simulator::Schedule (Seconds (4.0), &SendPacket, srcSocket2, iR2Dst.GetAddress (1), 5555, 512);

  Simulator::Stop (Seconds (5.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}