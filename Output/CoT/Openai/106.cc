#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv6-address-helper.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/ipv6-routing-table-entry.h"
#include "ns3/csma-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/packet-sink.h"
#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("Ipv6FragmentationPmtuExample");

/* Custom trace sink for packet receptions */
void RxTracePtr (std::string context, Ptr<const Packet> p, const Address &addr)
{
  static std::ofstream out ("fragmentation-ipv6-PMTU.tr", std::ios_base::app);
  out << Simulator::Now ().GetSeconds () << "s "
      << context << " PacketSize=" << p->GetSize () << "\n";
}

int main (int argc, char *argv[])
{
  LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
  LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);

  // Create nodes: Src--n0--r1--n1--r2--n2
  NodeContainer nodes;
  nodes.Create (6);
  // Naming
  Ptr<Node> src = nodes.Get(0);
  Ptr<Node> n0  = nodes.Get(1);
  Ptr<Node> r1  = nodes.Get(2);
  Ptr<Node> n1  = nodes.Get(3);
  Ptr<Node> r2  = nodes.Get(4);
  Ptr<Node> n2  = nodes.Get(5);

  // Node containers per segment
  NodeContainer seg0 (src, n0);
  NodeContainer seg1 (n0, r1, n1);
  NodeContainer seg2 (n1, r2, n2);

  // CSMA segment 1: n0, r1, n1 (MTU: 2000)
  CsmaHelper csma1;
  csma1.SetChannelAttribute ("DataRate", StringValue ("100Mbps"));
  csma1.SetChannelAttribute ("Delay", TimeValue (NanoSeconds (6560)));
  NetDeviceContainer ndc1 = csma1.Install (seg1);
  for (uint32_t i = 0; i < ndc1.GetN (); ++i)
    ndc1.Get (i)->SetMtu (2000);

  // P2P segment 0: src <-> n0 (MTU: 5000)
  PointToPointHelper p2p0;
  p2p0.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  p2p0.SetChannelAttribute ("Delay", StringValue ("2ms"));
  NetDeviceContainer ndc0 = p2p0.Install (src, n0);
  ndc0.Get (0)->SetMtu (5000);
  ndc0.Get (1)->SetMtu (5000);

  // P2P segment 2: n1 <-> r2 (MTU: 1500)
  PointToPointHelper p2p1;
  p2p1.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  p2p1.SetChannelAttribute ("Delay", StringValue ("5ms"));
  NetDeviceContainer ndc2_1 = p2p1.Install (n1, r2);
  ndc2_1.Get (0)->SetMtu (1500);
  ndc2_1.Get (1)->SetMtu (1500);

  // P2P n2 <-> r2 (MTU: 1500)
  PointToPointHelper p2p2;
  p2p2.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  p2p2.SetChannelAttribute ("Delay", StringValue ("5ms"));
  NetDeviceContainer ndc2_2 = p2p2.Install (n2, r2);
  ndc2_2.Get (0)->SetMtu (1500);
  ndc2_2.Get (1)->SetMtu (1500);

  // Install IPv6 stack
  InternetStackHelper stack;
  stack.Install (nodes);

  // Assign IPv6 addresses
  Ipv6AddressHelper ipv6;
  // src <-> n0
  ipv6.SetBase (Ipv6Address ("2001:1::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer i_src_n0 = ipv6.Assign (ndc0);
  i_src_n0.SetForwarding (0, true);
  i_src_n0.SetForwarding (1, true);
  i_src_n0.SetDefaultRouteInAllNodes (0);

  // n0, r1, n1 CSMA
  ipv6.SetBase (Ipv6Address ("2001:2::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer i_n0_r1_n1 = ipv6.Assign (ndc1);
  for (uint32_t i=0; i<i_n0_r1_n1.GetN (); ++i)
    i_n0_r1_n1.SetForwarding (i, true);

  // n1 <-> r2
  ipv6.SetBase (Ipv6Address ("2001:3::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer i_n1_r2 = ipv6.Assign (ndc2_1);
  i_n1_r2.SetForwarding (0, true);
  i_n1_r2.SetForwarding (1, true);

  // n2 <-> r2
  ipv6.SetBase (Ipv6Address ("2001:4::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer i_n2_r2 = ipv6.Assign (ndc2_2);
  i_n2_r2.SetForwarding (0, true);
  i_n2_r2.SetForwarding (1, true);
  i_n2_r2.SetDefaultRouteInAllNodes (0);

  // Set up static routing so src can reach n2 and vice-versa
  Ipv6StaticRoutingHelper ipv6RoutingHelper;
  
  // src: default route via n0
  Ptr<Ipv6StaticRouting> srcRouting = ipv6RoutingHelper.GetStaticRouting (src->GetObject<Ipv6> ());
  srcRouting->SetDefaultRoute (i_src_n0.GetAddress (1, 1), 1);

  // n0: default route via r1 (on CSMA)
  Ptr<Ipv6StaticRouting> n0Routing = ipv6RoutingHelper.GetStaticRouting (n0->GetObject<Ipv6> ());
  n0Routing->SetDefaultRoute (i_n0_r1_n1.GetAddress (1, 1), 2);

  // r1: routes to src via n0, n1 via CSMA, default to n1
  Ptr<Ipv6StaticRouting> r1Routing = ipv6RoutingHelper.GetStaticRouting (r1->GetObject<Ipv6> ());
  r1Routing->AddNetworkRouteTo (Ipv6Address ("2001:1::"), 64, i_n0_r1_n1.GetAddress (0, 1), 1);
  r1Routing->AddNetworkRouteTo (Ipv6Address ("2001:4::"), 64, i_n0_r1_n1.GetAddress (2, 1), 2);
  r1Routing->SetDefaultRoute (i_n0_r1_n1.GetAddress (2, 1), 2);

  // n1: routes to CSMA, r2 via P2P
  Ptr<Ipv6StaticRouting> n1Routing = ipv6RoutingHelper.GetStaticRouting (n1->GetObject<Ipv6> ());
  n1Routing->SetDefaultRoute (i_n1_r2.GetAddress (1, 1), 2);

  // r2: routes to n1, n2
  Ptr<Ipv6StaticRouting> r2Routing = ipv6RoutingHelper.GetStaticRouting (r2->GetObject<Ipv6> ());
  r2Routing->AddNetworkRouteTo (Ipv6Address ("2001:2::"), 64, i_n1_r2.GetAddress (0, 1), 1); // to CSMA via n1
  r2Routing->AddNetworkRouteTo (Ipv6Address ("2001:1::"), 64, i_n1_r2.GetAddress (0, 1), 1);
  r2Routing->AddNetworkRouteTo (Ipv6Address ("2001:3::"), 64, i_n1_r2.GetAddress (0, 1), 1);
  r2Routing->SetDefaultRoute (i_n2_r2.GetAddress (0, 1), 2);

  // n2: default route via r2
  Ptr<Ipv6StaticRouting> n2Routing = ipv6RoutingHelper.GetStaticRouting (n2->GetObject<Ipv6> ());
  n2Routing->SetDefaultRoute (i_n2_r2.GetAddress (1, 1), 1);

  // Applications
  // UDP Echo Server on n2
  uint16_t echoPort = 9;
  UdpEchoServerHelper echoServer (echoPort);
  ApplicationContainer serverApps = echoServer.Install (n2);
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  // UDP Echo Client on src targeting n2
  // The MTU bottleneck forces fragmentation for large packets; so pkt size >1500
  UdpEchoClientHelper echoClient (i_n2_r2.GetAddress (0, 1), echoPort);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (3));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (4000));

  ApplicationContainer clientApps = echoClient.Install (src);
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  // Enable packet captures/tracing
  csma1.EnablePcapAll ("fragmentation-ipv6-PMTU", true);
  p2p0.EnablePcapAll ("fragmentation-ipv6-PMTU", true);
  p2p1.EnablePcapAll ("fragmentation-ipv6-PMTU", true);
  p2p2.EnablePcapAll ("fragmentation-ipv6-PMTU", true);

  // Trace all packet receptions at UDP echo server
  Config::Connect ("/NodeList/*/ApplicationList/*/$ns3::UdpEchoServer/Rx", MakeCallback (&RxTracePtr));

  Simulator::Stop (Seconds (11.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}