#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-static-routing-helper.h"
#include <iostream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("MultiInterfaceStaticRoutingExample");

void PacketReceiveCallback (Ptr<Socket> socket)
{
  Address from;
  while (Ptr<Packet> packet = socket->RecvFrom (from))
    {
      InetSocketAddress address = InetSocketAddress::ConvertFrom (from);
      NS_LOG_UNCOND ("Packet received at " << Simulator::Now ().GetSeconds ()
        << "s from " << address.GetIpv4 ()
        << " on interface " << socket->GetBoundNetDevice ()->GetIfIndex ()
        << ", size = " << packet->GetSize ());
    }
}

void PacketSendCallback (Ptr<Socket> socket)
{
  NS_LOG_UNCOND ("Packet sent at " << Simulator::Now ().GetSeconds ()
    << "s from iface " << socket->GetBoundNetDevice ()->GetIfIndex ());
}

int main (int argc, char *argv[])
{
  LogComponentEnable ("MultiInterfaceStaticRoutingExample", LOG_LEVEL_INFO);
  LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_WARN);
  LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_WARN);

  // Create nodes: Src, R1, R2, Dst
  NodeContainer nodes;
  nodes.Create (4);
  Ptr<Node> src = nodes.Get (0);
  Ptr<Node> r1  = nodes.Get (1);
  Ptr<Node> r2  = nodes.Get (2);
  Ptr<Node> dst = nodes.Get (3);

  // Name them for clarity
  Names::Add ("SourceNode", src);
  Names::Add ("Router1", r1);
  Names::Add ("Router2", r2);
  Names::Add ("DestinationNode", dst);

  // Create four p2p links (Src<->R1, Src<->R2, R1<->Dst, R2<->Dst)
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

  // Link1: Src-R1
  NodeContainer nc_src_r1 (src, r1);
  NetDeviceContainer dev_src_r1 = p2p.Install (nc_src_r1);

  // Link2: Src-R2
  NodeContainer nc_src_r2 (src, r2);
  NetDeviceContainer dev_src_r2 = p2p.Install (nc_src_r2);

  // Link3: R1-Dst
  NodeContainer nc_r1_dst (r1, dst);
  NetDeviceContainer dev_r1_dst = p2p.Install (nc_r1_dst);

  // Link4: R2-Dst
  NodeContainer nc_r2_dst (r2, dst);
  NetDeviceContainer dev_r2_dst = p2p.Install (nc_r2_dst);

  // Install internet stack
  InternetStackHelper stack;
  stack.InstallAll ();

  // Assign IP addresses
  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.0.1.0", "255.255.255.0");
  Ipv4InterfaceContainer iface_src_r1 = ipv4.Assign (dev_src_r1);

  ipv4.SetBase ("10.0.2.0", "255.255.255.0");
  Ipv4InterfaceContainer iface_src_r2 = ipv4.Assign (dev_src_r2);

  ipv4.SetBase ("10.0.3.0", "255.255.255.0");
  Ipv4InterfaceContainer iface_r1_dst = ipv4.Assign (dev_r1_dst);

  ipv4.SetBase ("10.0.4.0", "255.255.255.0");
  Ipv4InterfaceContainer iface_r2_dst = ipv4.Assign (dev_r2_dst);

  // Bring up all interfaces (done by default)

  // Static Routing: Two routes from Source to Destination through R1 (lower metric) and R2 (higher metric)
  Ipv4StaticRoutingHelper staticRouting;

  Ptr<Ipv4> ipv4Src = src->GetObject<Ipv4> ();
  Ptr<Ipv4StaticRouting> staticSrc = staticRouting.GetStaticRouting (ipv4Src);

  // Route 1: via R1, metric 1, through src-r1 (iface 1)
  staticSrc->AddNetworkRouteTo (
    Ipv4Address ("10.0.3.0"), Ipv4Mask ("255.255.255.0"),
    Ipv4Address ("10.0.1.2"), 1, 1);

  staticSrc->AddNetworkRouteTo (
    Ipv4Address ("10.0.4.0"), Ipv4Mask ("255.255.255.0"),
    Ipv4Address ("10.0.2.2"), 2, 5);

  // Install static routes in routers
  // R1: route to dst via R1-Dst (iface 2)
  Ptr<Ipv4StaticRouting> staticR1 = staticRouting.GetStaticRouting (r1->GetObject<Ipv4> ());
  staticR1->AddNetworkRouteTo (Ipv4Address ("10.0.4.0"), Ipv4Mask ("255.255.255.0"),
                              Ipv4Address ("10.0.3.2"), 2);

  staticR1->AddNetworkRouteTo (Ipv4Address ("10.0.2.0"), Ipv4Mask ("255.255.255.0"),
                              Ipv4Address ("10.0.1.1"), 1);

  // R2: route to dst via R2-Dst (iface 2)
  Ptr<Ipv4StaticRouting> staticR2 = staticRouting.GetStaticRouting (r2->GetObject<Ipv4> ());
  staticR2->AddNetworkRouteTo (Ipv4Address ("10.0.3.0"), Ipv4Mask ("255.255.255.0"),
                              Ipv4Address ("10.0.4.2"), 2);

  staticR2->AddNetworkRouteTo (Ipv4Address ("10.0.1.0"), Ipv4Mask ("255.255.255.0"),
                              Ipv4Address ("10.0.2.1"), 1);

  // Destination: default routes back via both interfaces
  Ptr<Ipv4StaticRouting> staticDst = staticRouting.GetStaticRouting (dst->GetObject<Ipv4> ());
  staticDst->AddNetworkRouteTo (Ipv4Address ("0.0.0.0"), Ipv4Mask ("0.0.0.0"),
                               Ipv4Address ("10.0.3.1"), 1, 1);
  staticDst->AddNetworkRouteTo (Ipv4Address ("0.0.0.0"), Ipv4Mask ("0.0.0.0"),
                               Ipv4Address ("10.0.4.1"), 2, 2);

  // Bind receiving socket on destination, listening on all interfaces
  TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
  Ptr<Socket> recvSocketDst = Socket::CreateSocket (dst, tid);

  InetSocketAddress dstBindAddr (Ipv4Address::GetAny (), 5555);
  recvSocketDst->Bind (dstBindAddr);
  recvSocketDst->SetRecvCallback (MakeCallback (&PacketReceiveCallback));

  // Source: create two sending sockets, one per interface
  Ptr<Socket> sendSocket1 = Socket::CreateSocket (src, tid);
  Ptr<NetDevice> dev1 = src->GetDevice (1); // IfIndex 1: src<->r1
  Ptr<NetDevice> dev2 = src->GetDevice (2); // IfIndex 2: src<->r2

  sendSocket1->BindToNetDevice (dev1);
  sendSocket1->SetAllowBroadcast (true);

  Ptr<Socket> sendSocket2 = Socket::CreateSocket (src, tid);
  sendSocket2->BindToNetDevice (dev2);
  sendSocket2->SetAllowBroadcast (true);

  // Bind a receiver socket on the source on all interfaces (for replies)
  Ptr<Socket> recvSocketSrc = Socket::CreateSocket (src, tid);
  InetSocketAddress srcBindAddr (Ipv4Address::GetAny (), 5555);
  recvSocketSrc->Bind (srcBindAddr);
  recvSocketSrc->SetRecvCallback (MakeCallback (&PacketReceiveCallback));

  // Schedule sending UDP to destination from each interface
  Simulator::ScheduleWithContext (sendSocket1->GetNode ()->GetId (), Seconds (1.0),
    [sendSocket1, iface_r1_dst, dev1] () {
      InetSocketAddress dstAddr (iface_r1_dst.GetAddress (1), 5555);
      Ptr<Packet> pkt = Create<Packet> (100);
      sendSocket1->SetConnectCallback (MakeNullCallback<void, Ptr<Socket> > (), MakeNullCallback<void, Ptr<Socket> > ());
      sendSocket1->SendTo (pkt, 0, dstAddr);
      PacketSendCallback (sendSocket1);
    });

  Simulator::ScheduleWithContext (sendSocket2->GetNode ()->GetId (), Seconds (2.0),
    [sendSocket2, iface_r2_dst, dev2] () {
      InetSocketAddress dstAddr (iface_r2_dst.GetAddress (1), 5555);
      Ptr<Packet> pkt = Create<Packet> (100);
      sendSocket2->SetConnectCallback (MakeNullCallback<void, Ptr<Socket> > (), MakeNullCallback<void, Ptr<Socket> > ());
      sendSocket2->SendTo (pkt, 0, dstAddr);
      PacketSendCallback (sendSocket2);
    });

  Simulator::Stop (Seconds (4.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}