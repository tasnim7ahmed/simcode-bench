/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("ThreeRouterStaticRoutingExample");

int main (int argc, char *argv[])
{
  // Enable logging for debugging
  LogComponentEnable ("OnOffApplication", LOG_LEVEL_INFO);
  LogComponentEnable ("PacketSink", LOG_LEVEL_INFO);

  // General parameters
  std::string dataRate = "10Mbps";
  std::string delay = "5ms";
  uint16_t port = 8080;

  // Create three nodes: A, B, C
  NodeContainer nodes;
  nodes.Create (3);
  Ptr<Node> nodeA = nodes.Get (0);
  Ptr<Node> nodeB = nodes.Get (1);
  Ptr<Node> nodeC = nodes.Get (2);

  // Create point-to-point links
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue (dataRate));
  p2p.SetChannelAttribute ("Delay", StringValue (delay));

  // Link1: A <-> B
  NodeContainer link1 (nodeA, nodeB);
  NetDeviceContainer devices1 = p2p.Install (link1);

  // Link2: B <-> C
  NodeContainer link2 (nodeB, nodeC);
  NetDeviceContainer devices2 = p2p.Install (link2);

  // Install the Internet stack
  InternetStackHelper stack;
  stack.Install (nodes);

  // Assign IP addresses (using /32 per interface)
  Ipv4AddressHelper ipv4;
  // A-B interfaces
  ipv4.SetBase ("10.0.1.0", "255.255.255.255");
  Ipv4InterfaceContainer interfaces1 = ipv4.Assign (devices1);
  // B-C interfaces
  ipv4.SetBase ("10.0.2.0", "255.255.255.255");
  Ipv4InterfaceContainer interfaces2 = ipv4.Assign (devices2);

  Ipv4Address aAddress = interfaces1.GetAddress (0); // 10.0.1.1
  Ipv4Address bAddr_AB  = interfaces1.GetAddress (1); // 10.0.1.2
  Ipv4Address bAddr_BC  = interfaces2.GetAddress (0); // 10.0.2.1
  Ipv4Address cAddress = interfaces2.GetAddress (1); // 10.0.2.2

  // Set up static routing
  Ipv4StaticRoutingHelper staticRoutingHelper;

  // Node A: route to C (/32), next hop B's address on A-B link
  Ptr<Ipv4StaticRouting> aStaticRouting = staticRoutingHelper.GetStaticRouting (nodeA->GetObject<Ipv4> ());
  aStaticRouting->AddHostRouteTo (cAddress, bAddr_AB, 1);

  // Node B: route to C
  Ptr<Ipv4StaticRouting> bStaticRouting = staticRoutingHelper.GetStaticRouting (nodeB->GetObject<Ipv4> ());
  bStaticRouting->AddHostRouteTo (cAddress, cAddress, 2); // to C via B-C, out interface 2 (devices2.Get(0))
  // Node B: route back to A
  bStaticRouting->AddHostRouteTo (aAddress, aAddress, 1); // to A via A-B, out interface 1 (devices1.Get(1))

  // Node C: route back to A via B
  Ptr<Ipv4StaticRouting> cStaticRouting = staticRoutingHelper.GetStaticRouting (nodeC->GetObject<Ipv4> ());
  cStaticRouting->AddHostRouteTo (aAddress, bAddr_BC, 1);

  // UDP Server (PacketSink) on Node C
  Address sinkLocalAddress (InetSocketAddress (Ipv4Address::GetAny (), port));
  PacketSinkHelper sinkHelper ("ns3::UdpSocketFactory", sinkLocalAddress);
  ApplicationContainer sinkApp = sinkHelper.Install (nodeC);
  sinkApp.Start (Seconds (0.0));
  sinkApp.Stop (Seconds (10.0));

  // OnOff UDP Client on Node A
  OnOffHelper clientHelper ("ns3::UdpSocketFactory", Address (InetSocketAddress (cAddress, port)));
  clientHelper.SetAttribute ("DataRate", StringValue ("1Mbps"));
  clientHelper.SetAttribute ("PacketSize", UintegerValue (512));
  clientHelper.SetAttribute ("StartTime", TimeValue (Seconds (1.0)));
  clientHelper.SetAttribute ("StopTime", TimeValue (Seconds (9.0)));

  ApplicationContainer clientApps = clientHelper.Install (nodeA);

  // Enable tracing and pcap
  p2p.EnablePcapAll ("three-router-static-routing");

  // Enable ASCII tracing
  AsciiTraceHelper ascii;
  p2p.EnableAsciiAll (ascii.CreateFileStream ("three-router-static-routing.tr"));

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}