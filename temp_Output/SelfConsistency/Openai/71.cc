/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Three-Router Network Scenario with Global Routing in ns-3
 *
 * Topology:
 *    [A]---(P2P x.x.x.0/30)---[B]---(P2P y.y.y.0/30)---[C]
 *   /32  5Mbps, 2ms         /30      5Mbps, 2ms     /32
 *    |                                         |
 *  CSMA                                  CSMA
 * 172.16.1.1/32                     192.168.1.1/32
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("ThreeRouterGlobalRouting");

int
main (int argc, char *argv[])
{
  Time::SetResolution (Time::NS);

  // Create nodes: A, B, C
  NodeContainer nodes;
  nodes.Create (3);
  Ptr<Node> nodeA = nodes.Get (0);
  Ptr<Node> nodeB = nodes.Get (1);
  Ptr<Node> nodeC = nodes.Get (2);

  // Point-to-Point links: A<->B and B<->C
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

  // A <-> B
  NodeContainer ab (nodeA, nodeB);
  NetDeviceContainer devAB = p2p.Install (ab);

  // B <-> C
  NodeContainer bc (nodeB, nodeC);
  NetDeviceContainer devBC = p2p.Install (bc);

  // Add CSMA interfaces to A and C (single device per node, no channel connect)
  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", StringValue ("100Mbps"));
  csma.SetChannelAttribute ("Delay", TimeValue (NanoSeconds (6560)));

  // For single device, we create a "one-end" Csma link per host
  NodeContainer nodeAcsma (nodeA);
  NetDeviceContainer devAcsma = csma.Install (nodeAcsma);

  NodeContainer nodeCcsma (nodeC);
  NetDeviceContainer devCcsma = csma.Install (nodeCcsma);

  // Stack
  InternetStackHelper stack;
  stack.Install (nodes);

  // Assign IP Addresses

  // p2p AB: x.x.x.0/30 => 10.0.0.1 (A), 10.0.0.2 (B)
  Ipv4AddressHelper ipv4ab;
  ipv4ab.SetBase ("10.0.0.0", "255.255.255.252");
  Ipv4InterfaceContainer ifaceAB = ipv4ab.Assign (devAB);

  // p2p BC: y.y.y.0/30 => 10.0.0.5 (B), 10.0.0.6 (C)
  Ipv4AddressHelper ipv4bc;
  ipv4bc.SetBase ("10.0.0.4", "255.255.255.252");
  Ipv4InterfaceContainer ifaceBC = ipv4bc.Assign (devBC);

  // CSMA on A: 172.16.1.1/32
  Ipv4AddressHelper ipv4Acsma;
  ipv4Acsma.SetBase ("172.16.1.1", "255.255.255.255");
  Ipv4InterfaceContainer ifaceAcsma = ipv4Acsma.Assign (devAcsma);

  // CSMA on C: 192.168.1.1/32
  Ipv4AddressHelper ipv4Ccsma;
  ipv4Ccsma.SetBase ("192.168.1.1", "255.255.255.255");
  Ipv4InterfaceContainer ifaceCcsma = ipv4Ccsma.Assign (devCcsma);

  // Enable global routing
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // UDP traffic: OnOffApplication on A (source) to PacketSink at C (destination)
  uint16_t sinkPort = 9000;
  Address sinkAddress (InetSocketAddress (ifaceCcsma.GetAddress (0), sinkPort));

  PacketSinkHelper packetSinkHelper ("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), sinkPort));
  ApplicationContainer sinkApp = packetSinkHelper.Install (nodeC);
  sinkApp.Start (Seconds (0.0));
  sinkApp.Stop (Seconds (10.0));

  OnOffHelper onoff ("ns3::UdpSocketFactory", sinkAddress);
  onoff.SetAttribute ("PacketSize", UintegerValue (1024));
  onoff.SetAttribute ("DataRate", StringValue ("2Mbps"));        // Constant rate
  onoff.SetAttribute ("StartTime", TimeValue (Seconds (1.0)));
  onoff.SetAttribute ("StopTime", TimeValue (Seconds (9.0)));

  ApplicationContainer srcApp = onoff.Install (nodeA);

  // Enable tracing and pcap
  p2p.EnablePcapAll ("three-router");
  csma.EnablePcap ("three-router-A", devAcsma.Get (0), true);
  csma.EnablePcap ("three-router-C", devCcsma.Get (0), true);

  // ASCII Tracing
  AsciiTraceHelper ascii;
  Ptr<OutputStreamWrapper> stream = ascii.CreateFileStream ("three-router.tr");
  p2p.EnableAsciiAll (stream);

  Simulator::Stop (Seconds (10.1));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}