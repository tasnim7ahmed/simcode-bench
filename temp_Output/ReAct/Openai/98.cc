#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-interface-container.h"
#include "ns3/packet-sink.h"
#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("MixedP2pCsmaGlobalRouting");

void 
QueueEnqueueTrace(std::string context, Ptr<const Packet> packet)
{
  static std::ofstream enqueueLog("queue-enqueue.log", std::ios_base::app);
  enqueueLog << Simulator::Now ().GetSeconds () << " " << context << " Enqueue " << packet->GetSize () << std::endl;
}

void 
QueueDequeueTrace(std::string context, Ptr<const Packet> packet)
{
  static std::ofstream dequeueLog("queue-dequeue.log", std::ios_base::app);
  dequeueLog << Simulator::Now ().GetSeconds () << " " << context << " Dequeue " << packet->GetSize () << std::endl;
}

void 
PacketRecvTrace(Ptr<const Packet> packet, const Address &address)
{
  static std::ofstream recvLog("packet-recv.log", std::ios_base::app);
  recvLog << Simulator::Now ().GetSeconds () << " Packet received of size " << packet->GetSize () << std::endl;
}

void
RecomputeRoutes ()
{
  Ipv4GlobalRoutingHelper::RecomputeRoutingTables ();
}

int
main (int argc, char *argv[])
{
  LogComponentEnable ("MixedP2pCsmaGlobalRouting", LOG_LEVEL_INFO);

  // Create nodes
  NodeContainer nodes;
  nodes.Create (7); // n0, n1, n2, n3, n4, n5, n6

  // Topology:
  // n1<---p2pA--->n2<---csma--->n3<---p2pB--->n6
  //  \                                    /
  //   \--p2pC--->n4<---csma--->n5<---p2pD--/

  // P2P A: n1<->n2
  NodeContainer n1n2 = NodeContainer (nodes.Get(1), nodes.Get(2));

  // CSMA bus 1: n2<->n3<->n4
  NodeContainer csma1Nodes;
  csma1Nodes.Add(nodes.Get(2));
  csma1Nodes.Add(nodes.Get(3));
  csma1Nodes.Add(nodes.Get(4));

  // P2P B: n3<->n6
  NodeContainer n3n6 = NodeContainer (nodes.Get(3), nodes.Get(6));

  // P2P C: n1<->n4
  NodeContainer n1n4 = NodeContainer (nodes.Get(1), nodes.Get(4));

  // CSMA bus 2: n4<->n5
  NodeContainer csma2Nodes;
  csma2Nodes.Add(nodes.Get(4));
  csma2Nodes.Add(nodes.Get(5));

  // P2P D: n5<->n6
  NodeContainer n5n6 = NodeContainer (nodes.Get(5), nodes.Get(6));  

  // Install Internet Stack
  InternetStackHelper internet;
  internet.Install (nodes);

  // Point-to-Point configurations
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

  // CSMA configurations
  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", StringValue ("100Mbps"));
  csma.SetChannelAttribute ("Delay", TimeValue (NanoSeconds (6560)));

  // Install p2p devices
  NetDeviceContainer d1 = p2p.Install (n1n2);  // P2P A
  NetDeviceContainer d2 = p2p.Install (n3n6);  // P2P B
  NetDeviceContainer d3 = p2p.Install (n1n4);  // P2P C
  NetDeviceContainer d4 = p2p.Install (n5n6);  // P2P D

  // Install CSMA devices
  NetDeviceContainer csma1 = csma.Install (csma1Nodes);
  NetDeviceContainer csma2 = csma.Install (csma2Nodes);

  // Assign IP addresses
  Ipv4AddressHelper ipv4;
  // P2P A: n1<->n2
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i1 = ipv4.Assign (d1);

  // CSMA 1: n2<->n3<->n4
  ipv4.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer i2 = ipv4.Assign (csma1);

  // P2P B: n3<->n6
  ipv4.SetBase ("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer i3 = ipv4.Assign (d2);

  // P2P C: n1<->n4
  ipv4.SetBase ("10.1.4.0", "255.255.255.0");
  Ipv4InterfaceContainer i4 = ipv4.Assign (d3);

  // CSMA 2: n4<->n5
  ipv4.SetBase ("10.1.5.0", "255.255.255.0");
  Ipv4InterfaceContainer i5 = ipv4.Assign (csma2);

  // P2P D: n5<->n6
  ipv4.SetBase ("10.1.6.0", "255.255.255.0");
  Ipv4InterfaceContainer i6 = ipv4.Assign (d4);

  // Enable global routing and populate tables
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // Setup PacketSink on node 6
  uint16_t sinkPort = 8080;
  Address sinkAddress (InetSocketAddress (i3.GetAddress (1), sinkPort));
  PacketSinkHelper packetSinkHelper ("ns3::UdpSocketFactory", sinkAddress);
  ApplicationContainer sinkApp = packetSinkHelper.Install (nodes.Get (6));
  sinkApp.Start (Seconds (0.0));
  sinkApp.Stop (Seconds (25.0));
  // Trace sinks
  sinkApp.Get (0)->GetObject<PacketSink> ()->TraceConnectWithoutContext (
      "Rx", MakeCallback (&PacketRecvTrace));

  // Setup flow 1: node 1 to node 6 via n1->n2->n3->n6 over P2P A at 1s
  OnOffHelper onoff1 ("ns3::UdpSocketFactory", sinkAddress);
  onoff1.SetConstantRate (DataRate ("500kbps"));
  onoff1.SetAttribute ("PacketSize", UintegerValue (1024));
  ApplicationContainer app1 = onoff1.Install (nodes.Get (1));
  app1.Start (Seconds (1.0));
  app1.Stop (Seconds (10.0));

  // Setup flow 2: node 1 to node 6 via n1->n4->n5->n6 over P2P C/D at 11s (alternative path)
  // (Uses same sink)
  OnOffHelper onoff2 ("ns3::UdpSocketFactory", sinkAddress);
  onoff2.SetConstantRate (DataRate ("300kbps"));
  onoff2.SetAttribute ("PacketSize", UintegerValue (1024));
  ApplicationContainer app2 = onoff2.Install (nodes.Get (1));
  app2.Start (Seconds (11.0));
  app2.Stop (Seconds (20.0));

  // Trace queue activity for all PointToPointNetDevices
  Config::Connect ("/NodeList/*/DeviceList/*/$ns3::PointToPointNetDevice/TxQueue/Enqueue", MakeCallback (&QueueEnqueueTrace));
  Config::Connect ("/NodeList/*/DeviceList/*/$ns3::PointToPointNetDevice/TxQueue/Dequeue", MakeCallback (&QueueDequeueTrace));

  // Interface events to simulate failover and cause rerouting
  Ptr<Node> n2 = nodes.Get(2);
  Ptr<Node> n3 = nodes.Get(3);
  Ptr<Node> n4 = nodes.Get(4);
  Ptr<Node> n5 = nodes.Get(5);
  Ptr<Node> n1 = nodes.Get(1);
  Ptr<Node> n6 = nodes.Get(6);

  Ptr<Ipv4> ipv4_n1 = n1->GetObject<Ipv4> ();
  Ptr<Ipv4> ipv4_n2 = n2->GetObject<Ipv4> ();
  Ptr<Ipv4> ipv4_n3 = n3->GetObject<Ipv4> ();
  Ptr<Ipv4> ipv4_n4 = n4->GetObject<Ipv4> ();
  Ptr<Ipv4> ipv4_n5 = n5->GetObject<Ipv4> ();
  Ptr<Ipv4> ipv4_n6 = n6->GetObject<Ipv4> ();

  // Identify interface indices
  // n1: iface 1: p2p to n2 (d1.Get(0)), iface 2: p2p to n4 (d3.Get(0))
  uint32_t n1_p2pto_n2 = 1;
  uint32_t n1_p2pto_n4 = 2;
  // n2: iface 1: p2p to n1 (d1.Get(1)), iface 2: csma (csma1.Get(0))
  uint32_t n2_p2pto_n1 = 1;
  uint32_t n2_csma = 2;
  // n3: iface 1: csma (csma1.Get(1)), iface 2: p2p to n6 (d2.Get(0))
  uint32_t n3_csma = 1;
  uint32_t n3_p2pto_n6 = 2;
  // n4: iface 1: csma1 (csma1.Get(2)), iface 2: p2p to n1 (d3.Get(1)), iface 3: csma2 (csma2.Get(0))
  uint32_t n4_csma = 1;
  uint32_t n4_p2pto_n1 = 2;
  uint32_t n4_csma2 = 3;
  // n5: iface 1: csma2 (csma2.Get(1)), iface 2: p2p to n6 (d4.Get(0))
  uint32_t n5_csma2 = 1;
  uint32_t n5_p2pto_n6 = 2;
  // n6: iface 1: p2p to n3 (d2.Get(1)), iface 2: p2p to n5 (d4.Get(1))
  uint32_t n6_p2pfrom_n3 = 1;
  uint32_t n6_p2pfrom_n5 = 2;

  // Events: 
  // At 5s, bring down n1-n2 (p2pA) to force failover
  Simulator::Schedule (Seconds (5.0), &Ipv4::SetDown, ipv4_n1, n1_p2pto_n2);
  Simulator::Schedule (Seconds (5.0), &Ipv4::SetDown, ipv4_n2, n2_p2pto_n1);
  Simulator::Schedule (Seconds (5.1), &RecomputeRoutes);

  // At 8s, bring up n1-n2 (p2pA) to restore original path
  Simulator::Schedule (Seconds (8.0), &Ipv4::SetUp, ipv4_n1, n1_p2pto_n2);
  Simulator::Schedule (Seconds (8.0), &Ipv4::SetUp, ipv4_n2, n2_p2pto_n1);
  Simulator::Schedule (Seconds (8.1), &RecomputeRoutes);

  // At 13s, simulate failure of p2pC (n1-n4), rerouting second flow
  Simulator::Schedule (Seconds (13.0), &Ipv4::SetDown, ipv4_n1, n1_p2pto_n4);
  Simulator::Schedule (Seconds (13.0), &Ipv4::SetDown, ipv4_n4, n4_p2pto_n1);
  Simulator::Schedule (Seconds (13.1), &RecomputeRoutes);

  // At 16s, restore p2pC (n1-n4)
  Simulator::Schedule (Seconds (16.0), &Ipv4::SetUp, ipv4_n1, n1_p2pto_n4);
  Simulator::Schedule (Seconds (16.0), &Ipv4::SetUp, ipv4_n4, n4_p2pto_n1);
  Simulator::Schedule (Seconds (16.1), &RecomputeRoutes);

  Simulator::Stop (Seconds (25.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}