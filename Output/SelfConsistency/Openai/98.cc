/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Simulation Description:
 * - 7 nodes (n0..n6)
 * - Mixed point-to-point and CSMA topology
 * - Two CBR/UDP flows from n1 to n6. The first uses one p2p link (start at 1s).
 *   The second starts at 11s and uses the alternate path (routed via interface changes).
 * - At various times, interfaces go down/up, causing rerouting.
 * - Global routing is enabled, with route recomputation upon interface events.
 * - Packet reception and device queue activity are traced to pcap and ascii files.
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/traffic-control-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("GlobalRoutingMixedNetwork");

void
RecomputeRoutes ()
{
  Ptr<Ipv4> ipv4;
  for (NodeList::Iterator i = NodeList::Begin (); i != NodeList::End (); ++i)
    {
      ipv4 = (*i)->GetObject<Ipv4> ();
      if (ipv4)
        {
          ipv4->GetRoutingProtocol ()->NotifyInterfaceUp (1);
        }
    }
  Ipv4GlobalRoutingHelper::RecomputeRoutingTables ();
}

void
InterfaceDown (Ptr<Node> node, uint32_t interface)
{
  NS_LOG_INFO ("InterfaceDown: Node " << node->GetId () << " Interface " << interface << " at " << Simulator::Now ().GetSeconds () << "s");
  Ptr<Ipv4> ipv4 = node->GetObject<Ipv4> ();
  ipv4->SetDown (interface);
  Ipv4GlobalRoutingHelper::RecomputeRoutingTables ();
}

void
InterfaceUp (Ptr<Node> node, uint32_t interface)
{
  NS_LOG_INFO ("InterfaceUp: Node " << node->GetId () << " Interface " << interface << " at " << Simulator::Now ().GetSeconds () << "s");
  Ptr<Ipv4> ipv4 = node->GetObject<Ipv4> ();
  ipv4->SetUp (interface);
  Ipv4GlobalRoutingHelper::RecomputeRoutingTables ();
}

void
RxTraceFunction (Ptr<const Packet> packet, const Address &address)
{
  NS_LOG_UNCOND (Simulator::Now ().GetSeconds ()
                << "s Packet received at " << InetSocketAddress::ConvertFrom (address).GetIpv4 ());
}

void
QueueTraceFunction (Ptr<const QueueDiscItem> item)
{
  NS_LOG_UNCOND (Simulator::Now ().GetSeconds ()
                << "s Queue disc received a packet of size " << item->GetSize () << " bytes");
}

int
main (int argc, char *argv[])
{
  LogComponentEnable ("GlobalRoutingMixedNetwork", LOG_LEVEL_INFO);

  // 1. Create nodes
  NodeContainer nodes;
  nodes.Create (7); // n0..n6

  // 2. Create point-to-point and CSMA links
  // Topology:
  // n0---p2p---n2---csma---n3---p2p---n4---p2p---n6
  //             |                   |
  //           n1                  n5

  // For clarity, define specific NodeContainers for link endpoints

  // p2p links
  NodeContainer n0n2 = NodeContainer (nodes.Get (0), nodes.Get (2)); // n0<->n2
  NodeContainer n1n2 = NodeContainer (nodes.Get (1), nodes.Get (2)); // n1<->n2
  NodeContainer n3n4 = NodeContainer (nodes.Get (3), nodes.Get (4)); // n3<->n4
  NodeContainer n4n6 = NodeContainer (nodes.Get (4), nodes.Get (6)); // n4<->n6
  NodeContainer n5n4 = NodeContainer (nodes.Get (5), nodes.Get (4)); // n5<->n4

  // CSMA segment: n2, n3
  NodeContainer csmaNodes;
  csmaNodes.Add (nodes.Get (2));
  csmaNodes.Add (nodes.Get (3));

  // 3. Install network stacks
  InternetStackHelper stack;
  stack.Install (nodes);

  // 4. Set up device helpers
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", StringValue ("100Mbps"));
  csma.SetChannelAttribute ("Delay", TimeValue (NanoSeconds (6560)));

  // 5. Install NetDevices
  NetDeviceContainer d0d2 = p2p.Install (n0n2);
  NetDeviceContainer d1d2 = p2p.Install (n1n2);
  NetDeviceContainer d3d4 = p2p.Install (n3n4);
  NetDeviceContainer d4d6 = p2p.Install (n4n6);
  NetDeviceContainer d5d4 = p2p.Install (n5n4);

  NetDeviceContainer csmaDevs = csma.Install (csmaNodes);

  // 6. Assign IP addresses
  Ipv4AddressHelper address;
  Ipv4InterfaceContainer i0i2, i1i2, i3i4, i4i6, i5i4, i2i3;

  // 10.1.1.0/24 for n0<->n2
  address.SetBase ("10.1.1.0", "255.255.255.0");
  i0i2 = address.Assign (d0d2);

  // 10.1.2.0/24 for n1<->n2
  address.SetBase ("10.1.2.0", "255.255.255.0");
  i1i2 = address.Assign (d1d2);

  // 10.1.3.0/24 for n2<->n3 (CSMA, 2 nodes)
  address.SetBase ("10.1.3.0", "255.255.255.0");
  i2i3 = address.Assign (csmaDevs);

  // 10.1.4.0/24 for n3<->n4
  address.SetBase ("10.1.4.0", "255.255.255.0");
  i3i4 = address.Assign (d3d4);

  // 10.1.5.0/24 for n4<->n6
  address.SetBase ("10.1.5.0", "255.255.255.0");
  i4i6 = address.Assign (d4d6);

  // 10.1.6.0/24 for n5<->n4
  address.SetBase ("10.1.6.0", "255.255.255.0");
  i5i4 = address.Assign (d5d4);

  // 7. Enable global routing and respond to interface events
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();
  Config::SetDefault ("ns3::Ipv4GlobalRouting::RespondToInterfaceEvents", BooleanValue (true));

  // 8. Install traffic generators

  // First flow: starts at 1s, stops at 8s (on one p2p link)
  uint16_t port1 = 9000;
  OnOffHelper onoff1 ("ns3::UdpSocketFactory",
                      Address (InetSocketAddress (i4i6.GetAddress (1), port1))); // n6 receiver
  onoff1.SetConstantRate (DataRate ("1Mbps"), 512);
  onoff1.SetAttribute ("StartTime", TimeValue (Seconds (1.0)));
  onoff1.SetAttribute ("StopTime", TimeValue (Seconds (8.0)));

  ApplicationContainer app1 = onoff1.Install (nodes.Get (1)); // n1 sender

  // Second flow: starts at 11s, stops at 18s (after link change)
  uint16_t port2 = 9001;
  OnOffHelper onoff2 ("ns3::UdpSocketFactory",
                      Address (InetSocketAddress (i4i6.GetAddress (1), port2)));
  onoff2.SetConstantRate (DataRate ("1Mbps"), 512);
  onoff2.SetAttribute ("StartTime", TimeValue (Seconds (11.0)));
  onoff2.SetAttribute ("StopTime", TimeValue (Seconds (18.0)));

  ApplicationContainer app2 = onoff2.Install (nodes.Get (1));

  // Install PacketSink on n6 for both ports
  PacketSinkHelper sink1 ("ns3::UdpSocketFactory",
                          InetSocketAddress (Ipv4Address::GetAny (), port1));
  ApplicationContainer sinkApp1 = sink1.Install (nodes.Get (6));
  sinkApp1.Start (Seconds (0.0));
  sinkApp1.Stop (Seconds (30.0));

  PacketSinkHelper sink2 ("ns3::UdpSocketFactory",
                          InetSocketAddress (Ipv4Address::GetAny (), port2));
  ApplicationContainer sinkApp2 = sink2.Install (nodes.Get (6));
  sinkApp2.Start (Seconds (0.0));
  sinkApp2.Stop (Seconds (30.0));

  // 9. Schedule interface down/up events for rerouting
  // Suppose first path is: n1->n2->n3->n4->n6 (via CSMA, n3-n4 p2p, n4-n6 p2p)

  // To force route switch, bring down n3-n4 p2p link at 8s, up at 16s; 
  // similarly, can bring down or up other interfaces if desired.

  // Find interface indexes
  Ptr<Node> n3 = nodes.Get (3);
  Ptr<Node> n4 = nodes.Get (4);

  Ptr<Ipv4> ipv4_n3 = n3->GetObject<Ipv4> ();
  Ptr<Ipv4> ipv4_n4 = n4->GetObject<Ipv4> ();

  // Interfaces: 0: loopback; 1: p2p; 2: csma
  // To get mapping, print Ipv4 interfaces:
  // n3: 0 = loopback, 1 = p2p (n4), 2 = csma (n2)
  // n4: 0 = loopback, 1 = p2p (n3), 2 = p2p (n6), 3 = p2p (n5)

  // At 8s, bring down interface between n3 <-> n4 (on both sides)
  Simulator::Schedule (Seconds (8.0), &InterfaceDown, n3, 1);
  Simulator::Schedule (Seconds (8.0), &InterfaceDown, n4, 1);

  // At 16s, bring it back up
  Simulator::Schedule (Seconds (16.0), &InterfaceUp, n3, 1);
  Simulator::Schedule (Seconds (16.0), &InterfaceUp, n4, 1);

  // (Optional: can toggle other links/interfaces as needed)

  // 10. Output tracing
  // Enable pcap tracing for point-to-point and csma devices
  p2p.EnablePcapAll ("mixed-global-routing");
  csma.EnablePcapAll ("mixed-global-routing-csma");

  // Ascii tracing
  AsciiTraceHelper ascii;
  p2p.EnableAsciiAll (ascii.CreateFileStream ("mixed-global-routing.tr"));
  csma.EnableAsciiAll (ascii.CreateFileStream ("mixed-global-routing-csma.tr"));

  // Queue disc tracing (Optional, using TrafficControlHelper)
  TrafficControlHelper tch;
  tch.SetRootQueueDisc ("ns3::PfifoFastQueueDisc");
  for (uint32_t i = 0; i < d0d2.GetN (); ++i)
    {
      tch.Install (d0d2.Get (i));
    }
  for (uint32_t i = 0; i < d1d2.GetN (); ++i)
    {
      tch.Install (d1d2.Get (i));
    }
  for (uint32_t i = 0; i < d3d4.GetN (); ++i)
    {
      tch.Install (d3d4.Get (i));
    }
  for (uint32_t i = 0; i < d4d6.GetN (); ++i)
    {
      tch.Install (d4d6.Get (i));
    }
  for (uint32_t i = 0; i < d5d4.GetN (); ++i)
    {
      tch.Install (d5d4.Get (i));
    }

  // Connect trace sources for packet sinks
  for (uint32_t i = 0; i < sinkApp1.GetN (); ++i)
    {
      Ptr<Application> app = sinkApp1.Get (i);
      Ptr<PacketSink> sink = DynamicCast<PacketSink> (app);
      if (sink)
        {
          sink->TraceConnectWithoutContext ("Rx", MakeCallback (&RxTraceFunction));
        }
    }
  for (uint32_t i = 0; i < sinkApp2.GetN (); ++i)
    {
      Ptr<Application> app = sinkApp2.Get (i);
      Ptr<PacketSink> sink = DynamicCast<PacketSink> (app);
      if (sink)
        {
          sink->TraceConnectWithoutContext ("Rx", MakeCallback (&RxTraceFunction));
        }
    }

  // Schedule simulation stop
  Simulator::Stop (Seconds (22.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}