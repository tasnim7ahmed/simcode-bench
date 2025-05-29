/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * RIPng Routing Topology Simulation
 *
 *   SRC--A--B--D--DST
 *         \ /  /
 *          C---
 * Link costs:
 *  - All links: 1
 *  - C--D: 10
 *
 * Link BD is broken at 40s and restored at 44s.
 * Split Horizon mode configurable.
 * Static IPv6 addresses on A and D.
 * PingBetween SRC and DST.
 * Pcap and ascii tracing enabled.
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv6-address.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/ipv6-list-routing-helper.h"
#include "ns3/ipv6-routing-table-entry.h"
#include "ns3/ripng-helper.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv6-interface-container.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("RipngSplitHorizonExample");

int main (int argc, char **argv)
{
  bool enablePcap = true;
  RipNgHelper::SplitHorizonType splitHorizon = RipNgHelper::POISON_REVERSE;

  CommandLine cmd;
  cmd.AddValue ("pcap", "Enable PCAP tracing", enablePcap);
  cmd.Parse (argc, argv);

  // Node containers
  Ptr<Node> src = CreateObject<Node> ();
  Ptr<Node> a   = CreateObject<Node> ();
  Ptr<Node> b   = CreateObject<Node> ();
  Ptr<Node> c   = CreateObject<Node> ();
  Ptr<Node> d   = CreateObject<Node> ();
  Ptr<Node> dst = CreateObject<Node> ();

  NodeContainer nodes;
  nodes.Add (src);
  nodes.Add (a);
  nodes.Add (b);
  nodes.Add (c);
  nodes.Add (d);
  nodes.Add (dst);

  // Create all point-to-point channels and NetDeviceContainers
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

  // SRC <-> A
  NodeContainer n_src_a (src, a);
  NetDeviceContainer d_src_a = p2p.Install (n_src_a);

  // A <-> B
  NodeContainer n_a_b (a, b);
  NetDeviceContainer d_a_b = p2p.Install (n_a_b);

  // A <-> C
  NodeContainer n_a_c (a, c);
  NetDeviceContainer d_a_c = p2p.Install (n_a_c);

  // B <-> C
  NodeContainer n_b_c (b, c);
  NetDeviceContainer d_b_c = p2p.Install (n_b_c);

  // B <-> D
  NodeContainer n_b_d (b, d);
  NetDeviceContainer d_b_d = p2p.Install (n_b_d);

  // C <-> D (cost 10)
  PointToPointHelper p2p_highcost;
  p2p_highcost.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  p2p_highcost.SetChannelAttribute ("Delay", StringValue ("2ms")); // delay not cost, so cost will be set using metric
  NodeContainer n_c_d (c, d);
  NetDeviceContainer d_c_d = p2p_highcost.Install (n_c_d);

  // D <-> DST
  NodeContainer n_d_dst (d, dst);
  NetDeviceContainer d_d_dst = p2p.Install (n_d_dst);

  // Install Internet Stack and RIPng
  RipNgHelper ripng;
  ripng.ExcludeInterface (a, 0);   // Don't run RIPng on loopback
  ripng.ExcludeInterface (b, 0);
  ripng.ExcludeInterface (c, 0);
  ripng.ExcludeInterface (d, 0);

  ripng.Set ("SplitHorizon", EnumValue (splitHorizon));
  ripng.SetInterfaceMetric (c, 3, 10); // Set high cost for C<->D (on C's interface to D)

  InternetStackHelper internetStack;
  Ipv6ListRoutingHelper listRH;
  listRH.Add (ripng, 0);

  // On routers A, B, C, D install with RIPng
  NodeContainer routers = NodeContainer (a, b, c, d);
  internetStack.SetIpv6StackInstall (true);
  internetStack.SetRoutingHelper (listRH);
  internetStack.Install (routers);

  // On SRC and DST, just basic stack (no RIPng)
  InternetStackHelper basicStack;
  basicStack.SetIpv6StackInstall (true);
  basicStack.Install (src);
  basicStack.Install (dst);

  // Assign IPv6 addresses
  Ipv6AddressHelper ipv6;

  // For ease of routing, assign /64 to each subnet
  // SRC <-> A
  ipv6.SetBase (Ipv6Address ("2001:0:0:1::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer i_src_a = ipv6.Assign (d_src_a);
  i_src_a.SetForwarding (0, true);
  i_src_a.SetDefaultRouteInAllNodes (0);

  // A <-> B
  ipv6.SetBase (Ipv6Address ("2001:0:0:2::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer i_a_b = ipv6.Assign (d_a_b);

  // A <-> C
  ipv6.SetBase (Ipv6Address ("2001:0:0:3::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer i_a_c = ipv6.Assign (d_a_c);

  // B <-> C
  ipv6.SetBase (Ipv6Address ("2001:0:0:4::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer i_b_c = ipv6.Assign (d_b_c);

  // B <-> D
  ipv6.SetBase (Ipv6Address ("2001:0:0:5::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer i_b_d = ipv6.Assign (d_b_d);

  // C <-> D (cost 10)
  ipv6.SetBase (Ipv6Address ("2001:0:0:6::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer i_c_d = ipv6.Assign (d_c_d);

  // D <-> DST
  ipv6.SetBase (Ipv6Address ("2001:0:0:7::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer i_d_dst = ipv6.Assign (d_d_dst);
  i_d_dst.SetForwarding (0, true);
  i_d_dst.SetDefaultRouteInAllNodes (1);

  // Set static addresses for A and D
  // A's address on SRC-A: 2001:0:0:1::2
  i_src_a.SetForwarding (1, true);
  // D's address on D-DST: 2001:0:0:7::1
  i_d_dst.SetForwarding (0, true);

  // Manually assign default routes for SRC and DST
  Ptr<Ipv6StaticRouting> staticRoutingSrc = Ipv6RoutingHelper::GetRouting <Ipv6StaticRouting> (src->GetObject<Ipv6> ()->GetRoutingProtocol ());
  staticRoutingSrc->SetDefaultRoute (i_src_a.GetAddress (1,1), 1);

  Ptr<Ipv6StaticRouting> staticRoutingDst = Ipv6RoutingHelper::GetRouting <Ipv6StaticRouting> (dst->GetObject<Ipv6> ()->GetRoutingProtocol ());
  staticRoutingDst->SetDefaultRoute (i_d_dst.GetAddress (0,1), 1);

  // Enable global routing on all nodes (for static routes)
  Ipv6GlobalRoutingHelper::PopulateRoutingTables ();

  // Set up Ping6 from SRC to DST
  uint32_t packetSize = 1024;
  uint32_t maxPackets = 10;
  Time interPacketInterval = Seconds (1.);
  Ipv6Address dstAddr = i_d_dst.GetAddress (1,1);

  Ping6Helper ping6;
  ping6.SetLocal (i_src_a.GetAddress (0,1));
  ping6.SetRemote (dstAddr);
  ping6.SetAttribute ("MaxPackets", UintegerValue (maxPackets));
  ping6.SetAttribute ("Interval", TimeValue (interPacketInterval));
  ping6.SetAttribute ("PacketSize", UintegerValue (packetSize));
  ApplicationContainer apps = ping6.Install (src);
  apps.Start (Seconds (3.0));
  apps.Stop (Seconds (60.0));

  // Enable pcap and ascii tracing
  if (enablePcap)
    {
      p2p.EnablePcapAll ("ripng-split-horizon");
      p2p.EnableAsciiAll (CreateObject<AsciiTraceHelper> ("ripng-split-horizon.tr"));
      p2p_highcost.EnablePcapAll ("ripng-split-horizon-highcost");
      p2p_highcost.EnableAsciiAll (CreateObject<AsciiTraceHelper> ("ripng-split-horizon-highcost.tr"));
    }

  // Schedule BD link break at 40s, recovery at 44s
  Ptr<NetDevice> nd_b = d_b_d.Get (0); // On B
  Ptr<NetDevice> nd_d = d_b_d.Get (1); // On D

  Simulator::Schedule (Seconds (40.0), &NetDevice::SetLinkDown, nd_b);
  Simulator::Schedule (Seconds (40.0), &NetDevice::SetLinkDown, nd_d);
  Simulator::Schedule (Seconds (44.0), &NetDevice::SetLinkUp, nd_b);
  Simulator::Schedule (Seconds (44.0), &NetDevice::SetLinkUp, nd_d);

  // Print RIPng routing tables at 6s, 41s, and 45s
  RipNgHelper::PrintRoutingTableAllAt (Seconds (6.0), Create<OutputStreamWrapper> (&std::cout));
  RipNgHelper::PrintRoutingTableAllAt (Seconds (41.0), Create<OutputStreamWrapper> (&std::cout));
  RipNgHelper::PrintRoutingTableAllAt (Seconds (45.0), Create<OutputStreamWrapper> (&std::cout));

  Simulator::Stop (Seconds (60.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}