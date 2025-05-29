#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/netanim-module.h"
#include "ns3/queue-disc.h"
#include "ns3/traffic-control-module.h"
#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("MixedPtPAndCsmaRouting");

static void
RxTrace (Ptr<const Packet> packet, const Address &from)
{
  static std::ofstream rxLog ("packet-received.log", std::ios_base::app);
  rxLog << "Time: " << Simulator::Now().GetSeconds()
        << " Packet: " << packet->GetSize() << " from: " << InetSocketAddress::ConvertFrom (from).GetIpv4 () << std::endl;
}

static void
QueueEnqueueTrace (std::string context, Ptr<const Packet> p)
{
  static std::ofstream queueLog ("queue-activity.log", std::ios_base::app);
  queueLog << Simulator::Now ().GetSeconds ()
           << " Enqueue " << p->GetUid () << " " << context << std::endl;
}

static void
QueueDequeueTrace (std::string context, Ptr<const Packet> p)
{
  static std::ofstream queueLog ("queue-activity.log", std::ios_base::app);
  queueLog << Simulator::Now ().GetSeconds ()
           << " Dequeue " << p->GetUid () << " " << context << std::endl;
}

static void
RecomputeRoutes ()
{
  Ipv4GlobalRoutingHelper::RecomputeRoutingTables ();
}

int
main (int argc, char *argv[])
{
  LogComponentEnable ("MixedPtPAndCsmaRouting", LOG_LEVEL_INFO);

  // Node topology:
  // Nodes: n0--p2p--n1--csma--n2--p2p--n3
  //    \             |             /
  //     \----p2p-----+--csma------/
  //         n4              n5
  //                       /
  //                 n6 (receiver)
  //

  NodeContainer nodes;
  nodes.Create (7); // n0-n6

  // Point-to-point links between n0<->n1, n2<->n3, n0<->n4, n3<->n5
  PointToPointHelper p2p1;
  p2p1.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  p2p1.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer n0n1 = p2p1.Install (nodes.Get(0), nodes.Get(1));
  NetDeviceContainer n2n3 = p2p1.Install (nodes.Get(2), nodes.Get(3));
  NetDeviceContainer n0n4 = p2p1.Install (nodes.Get(0), nodes.Get(4));
  NetDeviceContainer n3n5 = p2p1.Install (nodes.Get(3), nodes.Get(5));
  NetDeviceContainer n1n2 = p2p1.Install (nodes.Get(1), nodes.Get(2)); // between csma islands

  // CSMA: n1, n5, n6
  CsmaHelper csmaA;
  csmaA.SetChannelAttribute ("DataRate", StringValue ("100Mbps"));
  csmaA.SetChannelAttribute ("Delay", TimeValue (NanoSeconds (6560)));

  NodeContainer csmaNodesA;
  csmaNodesA.Add (nodes.Get(1));
  csmaNodesA.Add (nodes.Get(5));
  csmaNodesA.Add (nodes.Get(6));
  NetDeviceContainer csmaDevsA = csmaA.Install (csmaNodesA);

  // Another CSMA: n2, n4, n5
  CsmaHelper csmaB;
  csmaB.SetChannelAttribute ("DataRate", StringValue ("100Mbps"));
  csmaB.SetChannelAttribute ("Delay", TimeValue (NanoSeconds (6560)));

  NodeContainer csmaNodesB;
  csmaNodesB.Add (nodes.Get(2));
  csmaNodesB.Add (nodes.Get(4));
  csmaNodesB.Add (nodes.Get(5));
  NetDeviceContainer csmaDevsB = csmaB.Install (csmaNodesB);

  // Install protocol stack
  InternetStackHelper stack;
  stack.Install (nodes);

  // Assign IP addresses
  Ipv4AddressHelper address;

  address.SetBase ("10.0.1.0", "255.255.255.0");
  Ipv4InterfaceContainer ip_n0n1 = address.Assign (n0n1);

  address.SetBase ("10.0.2.0", "255.255.255.0");
  Ipv4InterfaceContainer ip_n2n3 = address.Assign (n2n3);

  address.SetBase ("10.0.3.0", "255.255.255.0");
  Ipv4InterfaceContainer ip_n0n4 = address.Assign (n0n4);

  address.SetBase ("10.0.4.0", "255.255.255.0");
  Ipv4InterfaceContainer ip_n3n5 = address.Assign (n3n5);

  address.SetBase ("10.0.5.0", "255.255.255.0");
  Ipv4InterfaceContainer ip_csmaA = address.Assign (csmaDevsA);

  address.SetBase ("10.0.6.0", "255.255.255.0");
  Ipv4InterfaceContainer ip_csmaB = address.Assign (csmaDevsB);

  address.SetBase ("10.0.7.0", "255.255.255.0");
  Ipv4InterfaceContainer ip_n1n2 = address.Assign (n1n2);

  // Enable global routing
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // Set up queues tracing for all p2p
  Config::Connect ("/NodeList/*/DeviceList/*/$ns3::PointToPointNetDevice/TxQueue/Enqueue",
                   MakeBoundCallback (&QueueEnqueueTrace));
  Config::Connect ("/NodeList/*/DeviceList/*/$ns3::PointToPointNetDevice/TxQueue/Dequeue",
                   MakeBoundCallback (&QueueDequeueTrace));
  // Also for CSMA
  Config::Connect ("/NodeList/*/DeviceList/*/$ns3::CsmaNetDevice/TxQueue/Enqueue",
                   MakeBoundCallback (&QueueEnqueueTrace));
  Config::Connect ("/NodeList/*/DeviceList/*/$ns3::CsmaNetDevice/TxQueue/Dequeue",
                   MakeBoundCallback (&QueueDequeueTrace));

  // Set up UDP echo servers at node 6
  uint16_t udpPort1 = 9001, udpPort2 = 9002;
  UdpServerHelper server1 (udpPort1);
  ApplicationContainer serverApp1 = server1.Install (nodes.Get (6));
  serverApp1.Start (Seconds (0.0));
  serverApp1.Stop (Seconds (25.0));

  UdpServerHelper server2 (udpPort2);
  ApplicationContainer serverApp2 = server2.Install (nodes.Get (6));
  serverApp2.Start (Seconds (0.0));
  serverApp2.Stop (Seconds (25.0));

  // Set up UDP client applications at node 1 destined for node 6

  // Find node 6's address on csmaA
  Ipv4Address dstAddr = ip_csmaA.GetAddress (2);

  UdpClientHelper client1 (dstAddr, udpPort1);
  client1.SetAttribute ("MaxPackets", UintegerValue (10000));
  client1.SetAttribute ("Interval", TimeValue (MilliSeconds (20)));
  client1.SetAttribute ("PacketSize", UintegerValue (512));
  ApplicationContainer clientApp1 = client1.Install (nodes.Get (1));
  clientApp1.Start (Seconds (1.0));
  clientApp1.Stop (Seconds (10.0));

  UdpClientHelper client2 (dstAddr, udpPort2);
  client2.SetAttribute ("MaxPackets", UintegerValue (10000));
  client2.SetAttribute ("Interval", TimeValue (MilliSeconds (20)));
  client2.SetAttribute ("PacketSize", UintegerValue (512));
  ApplicationContainer clientApp2 = client2.Install (nodes.Get (1));
  clientApp2.Start (Seconds (11.0));
  clientApp2.Stop (Seconds (20.0));

  // Trace receptions at server
  Ptr<Node> node6 = nodes.Get (6);
  for (uint32_t i=0; i < node6->GetNDevices (); ++i)
    {
      node6->GetDevice (i)->GetObject<NetDevice> ()->TraceConnectWithoutContext ("MacRx", MakeCallback (&RxTrace));
    }

  // Simulate routing changes via interface down/up events

  Ptr<Node> n1 = nodes.Get (1);
  Ptr<Node> n2 = nodes.Get (2);

  // At t=5s, bring the n0-n1 link down (simulate failure)
  Simulator::Schedule (Seconds (5.0), [n1, ip_n0n1]()
  {
    Ptr<Ipv4> ipv4 = n1->GetObject<Ipv4> ();
    uint32_t ifIndex = ipv4->GetInterfaceForDevice (ip_n0n1.Get (1));
    ipv4->SetDown (ifIndex);
    RecomputeRoutes ();
  });

  // At t=8s, bring n0-n1 back up
  Simulator::Schedule (Seconds (8.0), [n1, ip_n0n1]()
  {
    Ptr<Ipv4> ipv4 = n1->GetObject<Ipv4> ();
    uint32_t ifIndex = ipv4->GetInterfaceForDevice (ip_n0n1.Get (1));
    ipv4->SetUp (ifIndex);
    RecomputeRoutes ();
  });

  // At t=13s, bring down n1-n2
  Simulator::Schedule (Seconds (13.0), [n1, ip_n1n2]()
  {
    Ptr<Ipv4> ipv4 = n1->GetObject<Ipv4> ();
    uint32_t ifIndex = ipv4->GetInterfaceForDevice (ip_n1n2.Get (0));
    ipv4->SetDown (ifIndex);
    RecomputeRoutes ();
  });

  // At t=16s, bring n1-n2 back up
  Simulator::Schedule (Seconds (16.0), [n1, ip_n1n2]()
  {
    Ptr<Ipv4> ipv4 = n1->GetObject<Ipv4> ();
    uint32_t ifIndex = ipv4->GetInterfaceForDevice (ip_n1n2.Get (0));
    ipv4->SetUp (ifIndex);
    RecomputeRoutes ();
  });

  Simulator::Stop (Seconds (25.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}