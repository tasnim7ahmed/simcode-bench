#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-address-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("MultiInterfaceStaticRoutingExample");

int main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  // Enable logging
  LogComponentEnable ("MultiInterfaceStaticRoutingExample", LOG_LEVEL_INFO);

  // Create nodes: src, rtr1, rtr2, dstRtr, dst
  NodeContainer src, rtr1, rtr2, dstRtr, dst;
  src.Create (1);
  rtr1.Create (1);
  rtr2.Create (1);
  dstRtr.Create (1);
  dst.Create (1);

  // PointToPoint helpers
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

  // Source to Rtr1 link
  NodeContainer srcRtr1 = NodeContainer (src.Get (0), rtr1.Get (0));
  NetDeviceContainer d_srcRtr1 = p2p.Install (srcRtr1);

  // Source to Rtr2 link
  NodeContainer srcRtr2 = NodeContainer (src.Get (0), rtr2.Get (0));
  NetDeviceContainer d_srcRtr2 = p2p.Install (srcRtr2);

  // Rtr1 to DstRtr
  NodeContainer rtr1DstRtr = NodeContainer (rtr1.Get (0), dstRtr.Get (0));
  NetDeviceContainer d_rtr1DstRtr = p2p.Install (rtr1DstRtr);

  // Rtr2 to DstRtr
  NodeContainer rtr2DstRtr = NodeContainer (rtr2.Get (0), dstRtr.Get (0));
  NetDeviceContainer d_rtr2DstRtr = p2p.Install (rtr2DstRtr);

  // DstRtr to Dst
  NodeContainer dstRtrDst = NodeContainer (dstRtr.Get (0), dst.Get (0));
  NetDeviceContainer d_dstRtrDst = p2p.Install (dstRtrDst);

  // Install Internet stack
  InternetStackHelper stack;
  stack.Install (src);
  stack.Install (rtr1);
  stack.Install (rtr2);
  stack.Install (dstRtr);
  stack.Install (dst);

  // IP address assignment
  Ipv4AddressHelper address;

  // Net 1: Src - Rtr1
  address.SetBase ("10.0.1.0", "255.255.255.0");
  Ipv4InterfaceContainer if_srcRtr1 = address.Assign (d_srcRtr1);

  // Net 2: Src - Rtr2
  address.SetBase ("10.0.2.0", "255.255.255.0");
  Ipv4InterfaceContainer if_srcRtr2 = address.Assign (d_srcRtr2);

  // Net 3: Rtr1 - DstRtr
  address.SetBase ("10.0.3.0", "255.255.255.0");
  Ipv4InterfaceContainer if_rtr1DstRtr = address.Assign (d_rtr1DstRtr);

  // Net 4: Rtr2 - DstRtr
  address.SetBase ("10.0.4.0", "255.255.255.0");
  Ipv4InterfaceContainer if_rtr2DstRtr = address.Assign (d_rtr2DstRtr);

  // Net 5: DstRtr - Dst
  address.SetBase ("10.0.5.0", "255.255.255.0");
  Ipv4InterfaceContainer if_dstRtrDst = address.Assign (d_dstRtrDst);

  // Set up static routing
  Ptr<Ipv4> ipv4_src = src.Get (0)->GetObject<Ipv4> ();
  Ptr<Ipv4StaticRouting> staticRouting_src = Ipv4RoutingHelper::GetRouting <Ipv4StaticRouting> (ipv4_src->GetRoutingProtocol ());

  Ptr<Ipv4> ipv4_rtr1 = rtr1.Get (0)->GetObject<Ipv4> ();
  Ptr<Ipv4StaticRouting> staticRouting_rtr1 = Ipv4RoutingHelper::GetRouting <Ipv4StaticRouting> (ipv4_rtr1->GetRoutingProtocol ());

  Ptr<Ipv4> ipv4_rtr2 = rtr2.Get (0)->GetObject<Ipv4> ();
  Ptr<Ipv4StaticRouting> staticRouting_rtr2 = Ipv4RoutingHelper::GetRouting <Ipv4StaticRouting> (ipv4_rtr2->GetRoutingProtocol ());

  Ptr<Ipv4> ipv4_dstRtr = dstRtr.Get (0)->GetObject<Ipv4> ();
  Ptr<Ipv4StaticRouting> staticRouting_dstRtr = Ipv4RoutingHelper::GetRouting <Ipv4StaticRouting> (ipv4_dstRtr->GetRoutingProtocol ());

  Ptr<Ipv4> ipv4_dst = dst.Get (0)->GetObject<Ipv4> ();
  Ptr<Ipv4StaticRouting> staticRouting_dst = Ipv4RoutingHelper::GetRouting <Ipv4StaticRouting> (ipv4_dst->GetRoutingProtocol ());

  // Default routes for host and destination
  staticRouting_src->SetDefaultRoute ("0.0.0.0", 0);
  staticRouting_dst->SetDefaultRoute (if_dstRtrDst.GetAddress (0), 1);

  // Static routes to reach destination (10.0.5.2)
  Ipv4Address dstAddr = if_dstRtrDst.GetAddress (1);

  // Let src send via Rtr1 (Net 1 and Net 3)
  staticRouting_src->AddNetworkRouteTo (
      Ipv4Address ("10.0.5.0"), Ipv4Mask ("255.255.255.0"),
      if_srcRtr1.GetAddress (1), 1);

  staticRouting_rtr1->AddNetworkRouteTo (
      Ipv4Address ("10.0.5.0"), Ipv4Mask ("255.255.255.0"),
      if_rtr1DstRtr.GetAddress (1), 2);

  staticRouting_rtr2->AddNetworkRouteTo (
      Ipv4Address ("10.0.5.0"), Ipv4Mask ("255.255.255.0"),
      if_rtr2DstRtr.GetAddress (1), 1);

  staticRouting_dstRtr->AddNetworkRouteTo (
      Ipv4Address ("10.0.5.0"), Ipv4Mask ("255.255.255.0"),
      if_dstRtrDst.GetAddress (1), 3);

  // Additional route on src to dst via Rtr2 (Net 2 and Net 4)
  staticRouting_src->AddNetworkRouteTo (
      Ipv4Address ("10.0.5.0"), Ipv4Mask ("255.255.255.0"),
      if_srcRtr2.GetAddress (1), 2);

  // Configure routes for reply/return path (default routes via Src to Rtr1)
  // Add routes on rtr1 for src
  staticRouting_rtr1->AddNetworkRouteTo (
      Ipv4Address ("10.0.1.0"), Ipv4Mask ("255.255.255.0"),
      if_srcRtr1.GetAddress (0), 1);
  // Add routes on rtr2 for src
  staticRouting_rtr2->AddNetworkRouteTo (
      Ipv4Address ("10.0.2.0"), Ipv4Mask ("255.255.255.0"),
      if_srcRtr2.GetAddress (0), 2);
  // Add routes on dstRtr for dst
  staticRouting_dstRtr->AddNetworkRouteTo (
      Ipv4Address ("10.0.5.0"), Ipv4Mask ("255.255.255.0"),
      if_dstRtrDst.GetAddress (1), 3);

  // Populate routing tables
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // TCP Server (sink) at destination
  uint16_t port = 50000;
  Address sinkAddress (InetSocketAddress (dstAddr, port));
  PacketSinkHelper packetSinkHelper ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), port));
  ApplicationContainer sinkApp = packetSinkHelper.Install (dst.Get (0));
  sinkApp.Start (Seconds (0.0));
  sinkApp.Stop (Seconds (10.0));

  // TCP Client at source
  // (1) Send via Rtr1 path
  Ptr<Socket> tcpSrc1 = Socket::CreateSocket (src.Get (0), TcpSocketFactory::GetTypeId ());
  Ptr<Packet> pkt1 = Create<Packet> (1024);
  tcpSrc1->Bind (InetSocketAddress (if_srcRtr1.GetAddress (0), 0));
  tcpSrc1->Connect (InetSocketAddress (dstAddr, port));

  Simulator::Schedule (Seconds (1.0), [&] () {
    NS_LOG_INFO ("Sending first packet via Rtr1");
    tcpSrc1->Send (pkt1);
  });

  // (2) Send via Rtr2 path (using the other interface)
  Ptr<Socket> tcpSrc2 = Socket::CreateSocket (src.Get (0), TcpSocketFactory::GetTypeId ());
  Ptr<Packet> pkt2 = Create<Packet> (1024);
  tcpSrc2->Bind (InetSocketAddress (if_srcRtr2.GetAddress (0), 0));
  tcpSrc2->Connect (InetSocketAddress (dstAddr, port));

  Simulator::Schedule (Seconds (3.0), [&] () {
    NS_LOG_INFO ("Sending second packet via Rtr2");
    tcpSrc2->Send (pkt2);
  });

  // Enable tracing
  p2p.EnablePcapAll ("multi-interface-static-routing");

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}