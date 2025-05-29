#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("MultiInterfaceStaticRoutingExample");

void
RxSrc (Ptr<Socket> socket)
{
  Ptr<Packet> packet;
  Address from;
  while ((packet = socket->RecvFrom (from)))
    {
      NS_LOG_INFO ("Source Received one packet, size: " << packet->GetSize ()
                   << " from " << InetSocketAddress::ConvertFrom (from).GetIpv4 ());
    }
}

void
RxDst (Ptr<Socket> socket)
{
  Ptr<Packet> packet;
  Address from;
  while ((packet = socket->RecvFrom (from)))
    {
      NS_LOG_INFO ("Destination Received one packet, size: " << packet->GetSize ()
                   << " from " << InetSocketAddress::ConvertFrom (from).GetIpv4 ());
    }
}

void
SendPacket (Ptr<Socket> sock, Address dst, uint32_t size)
{
  Ptr<Packet> packet = Create<Packet> (size);
  sock->SendTo (packet, 0, dst);
}

int
main (int argc, char *argv[])
{
  LogComponentEnable ("MultiInterfaceStaticRoutingExample", LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create (4); // 0:Source, 1:Router1, 2:Router2, 3:Destination

  // Source will have two interfaces
  NodeContainer srcToRtr1 (nodes.Get(0), nodes.Get(1));
  NodeContainer srcToRtr2 (nodes.Get(0), nodes.Get(2));
  NodeContainer rtr1ToDst (nodes.Get(1), nodes.Get(3));
  NodeContainer rtr2ToDst (nodes.Get(2), nodes.Get(3));

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

  // NetDevices
  NetDeviceContainer nd_src_rtr1 = p2p.Install (srcToRtr1);
  NetDeviceContainer nd_src_rtr2 = p2p.Install (srcToRtr2);
  NetDeviceContainer nd_rtr1_dst = p2p.Install (rtr1ToDst);
  NetDeviceContainer nd_rtr2_dst = p2p.Install (rtr2ToDst);

  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper ipv4;

  // SRC <-> RTR1: 10.0.1.0/24
  ipv4.SetBase ("10.0.1.0", "255.255.255.0");
  Ipv4InterfaceContainer if_src_rtr1 = ipv4.Assign (nd_src_rtr1);

  // SRC <-> RTR2: 10.0.2.0/24
  ipv4.SetBase ("10.0.2.0", "255.255.255.0");
  Ipv4InterfaceContainer if_src_rtr2 = ipv4.Assign (nd_src_rtr2);

  // RTR1 <-> DST: 10.0.3.0/24
  ipv4.SetBase ("10.0.3.0", "255.255.255.0");
  Ipv4InterfaceContainer if_rtr1_dst = ipv4.Assign (nd_rtr1_dst);

  // RTR2 <-> DST: 10.0.4.0/24
  ipv4.SetBase ("10.0.4.0", "255.255.255.0");
  Ipv4InterfaceContainer if_rtr2_dst = ipv4.Assign (nd_rtr2_dst);

  // Assign variable names for IPs
  Ipv4Address srcIf1 = if_src_rtr1.GetAddress (0); // Node 0, if1
  Ipv4Address srcIf2 = if_src_rtr2.GetAddress (0); // Node 0, if2
  Ipv4Address dstIp = if_rtr1_dst.GetAddress (1); // Node 3, first interface (via rtr1)
  Ipv4Address dstIp2 = if_rtr2_dst.GetAddress (1); // Node 3, second interface (via rtr2)

  // Static Routing: Source (Node 0)
  Ptr<Ipv4StaticRouting> srcStatic = Ipv4RoutingHelper::GetRouting <Ipv4StaticRouting> (nodes.Get(0)->GetObject<Ipv4> ()->GetRoutingProtocol ());
  // Route to dest via rtr1 (lower metric)
  srcStatic->AddNetworkRouteTo (
    Ipv4Address ("10.0.3.0"), Ipv4Mask ("255.255.255.0"),
    if_src_rtr1.GetAddress (1), 1, 1); // metric 1, out interface 1
  // Route to dest via rtr2 (higher metric)
  srcStatic->AddNetworkRouteTo (
    Ipv4Address ("10.0.4.0"), Ipv4Mask ("255.255.255.0"),
    if_src_rtr2.GetAddress (1), 2, 10); // metric 10, out interface 2

  // Static Routing: Router 1 (Node 1)
  Ptr<Ipv4StaticRouting> rtr1Static = Ipv4RoutingHelper::GetRouting <Ipv4StaticRouting> (nodes.Get(1)->GetObject<Ipv4> ()->GetRoutingProtocol ());
  rtr1Static->AddNetworkRouteTo (
    Ipv4Address ("10.0.4.0"), Ipv4Mask ("255.255.255.0"),
    if_rtr1_dst.GetAddress (1), 2, 1);
  rtr1Static->AddNetworkRouteTo (
    Ipv4Address ("10.0.2.0"), Ipv4Mask ("255.255.255.0"),
    if_src_rtr1.GetAddress (0), 1, 1);

  // Static Routing: Router 2 (Node 2)
  Ptr<Ipv4StaticRouting> rtr2Static = Ipv4RoutingHelper::GetRouting <Ipv4StaticRouting> (nodes.Get(2)->GetObject<Ipv4> ()->GetRoutingProtocol ());
  rtr2Static->AddNetworkRouteTo (
    Ipv4Address ("10.0.3.0"), Ipv4Mask ("255.255.255.0"),
    if_rtr2_dst.GetAddress (1), 2, 1);
  rtr2Static->AddNetworkRouteTo (
    Ipv4Address ("10.0.1.0"), Ipv4Mask ("255.255.255.0"),
    if_src_rtr2.GetAddress (0), 1, 1);

  // Static Routing: Destination (Node 3)
  Ptr<Ipv4StaticRouting> dstStatic = Ipv4RoutingHelper::GetRouting <Ipv4StaticRouting> (nodes.Get(3)->GetObject<Ipv4> ()->GetRoutingProtocol ());
  dstStatic->AddNetworkRouteTo (
    Ipv4Address ("10.0.1.0"), Ipv4Mask ("255.255.255.0"),
    if_rtr1_dst.GetAddress (0), 1, 1);
  dstStatic->AddNetworkRouteTo (
    Ipv4Address ("10.0.2.0"), Ipv4Mask ("255.255.255.0"),
    if_rtr2_dst.GetAddress (0), 2, 1);

  // UDP sockets at destination
  TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
  Ptr<Socket> recvSink = Socket::CreateSocket (nodes.Get (3), tid);
  InetSocketAddress local = InetSocketAddress (dstIp, 9000);
  recvSink->Bind (local);
  recvSink->SetRecvCallback (MakeCallback (&RxDst));

  // UDP sockets at source, per interface for dynamic binding
  Ptr<Socket> srcSock1 = Socket::CreateSocket (nodes.Get (0), tid);
  InetSocketAddress srcBind1 = InetSocketAddress (srcIf1, 0); // Bind to interface 1
  srcSock1->Bind (srcBind1);

  Ptr<Socket> srcSock2 = Socket::CreateSocket (nodes.Get (0), tid);
  InetSocketAddress srcBind2 = InetSocketAddress (srcIf2, 0); // Bind to interface 2
  srcSock2->Bind (srcBind2);

  // Add receive callback at source to receive replies (for demonstration)
  srcSock1->SetRecvCallback (MakeCallback (&RxSrc));
  srcSock2->SetRecvCallback (MakeCallback (&RxSrc));

  // Send UDP packets from source to destination using both interfaces
  Simulator::Schedule (Seconds (1.0), &SendPacket, srcSock1,
                       InetSocketAddress (dstIp, 9000), 200);
  Simulator::Schedule (Seconds (1.2), &SendPacket, srcSock2,
                       InetSocketAddress (dstIp, 9000), 200);

  Simulator::Stop (Seconds (3.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}