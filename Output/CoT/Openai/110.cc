#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv6-address-helper.h"
#include "ns3/ipv6-ping-helper.h"
#include "ns3/queue.h"
#include "ns3/packet.h"

using namespace ns3;

static void
QueueDepthTracer (Ptr<Queue<Packet>> queue, std::string context)
{
  uint32_t qlen = queue->GetNPackets ();
  std::ofstream out ("ping6.tr", std::ios::app);
  out << Simulator::Now ().GetSeconds () << " " << context << " QueueLength: " << qlen << std::endl;
  out.close ();
}

static void
PacketReceptionTracer (Ptr<const Packet> packet, Ptr<Ipv6> ipv6, uint32_t interface)
{
  std::ofstream out ("ping6.tr", std::ios::app);
  out << Simulator::Now ().GetSeconds () << " Ipv6PktRx: " << packet->GetSize () << " bytes on interface " << interface << std::endl;
  out.close ();
}

int
main (int argc, char *argv[])
{
  uint32_t nPackets = 5;
  double interval = 1.0;
  double stopTime = 10.0;

  CommandLine cmd;
  cmd.AddValue ("nPackets", "Number of ICMPv6 Echo Requests to send", nPackets);
  cmd.AddValue ("interval", "Interval between Echo Requests", interval);
  cmd.AddValue ("stopTime", "Simulation stop time", stopTime);
  cmd.Parse (argc, argv);

  // Create nodes
  NodeContainer nodes;
  nodes.Create (2);

  // Configure CSMA
  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", DataRateValue (DataRate ("100Mbps")));
  csma.SetChannelAttribute ("Delay", TimeValue (NanoSeconds (6560)));
  csma.SetQueue ("ns3::DropTailQueue<Packet>");
  NetDeviceContainer devices = csma.Install (nodes);

  // Install Internet stack with IPv6
  InternetStackHelper internetv6;
  internetv6.Install (nodes);

  // Assign IPv6 Addresses
  Ipv6AddressHelper ipv6;
  ipv6.SetBase (Ipv6Address ("2001:db8::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer interfaces = ipv6.Assign (devices);
  interfaces.SetForwarding (0, true);
  interfaces.SetDefaultRouteInAllNodes (0);

  // Tracing Queue Depths
  Ptr<NetDevice> nd0 = devices.Get (0);
  Ptr<CsmaNetDevice> csmaNd0 = DynamicCast<CsmaNetDevice> (nd0);
  Ptr<Queue<Packet>> queue0 = csmaNd0->GetQueue ();
  Simulator::ScheduleNow (&QueueDepthTracer, queue0, "n0");

  Ptr<NetDevice> nd1 = devices.Get (1);
  Ptr<CsmaNetDevice> csmaNd1 = DynamicCast<CsmaNetDevice> (nd1);
  Ptr<Queue<Packet>> queue1 = csmaNd1->GetQueue ();
  Simulator::ScheduleNow (&QueueDepthTracer, queue1, "n1");

  queue0->TraceConnectWithoutContext ("PacketsInQueue", MakeBoundCallback (&QueueDepthTracer, queue0, "n0"));
  queue1->TraceConnectWithoutContext ("PacketsInQueue", MakeBoundCallback (&QueueDepthTracer, queue1, "n1"));

  // Tracing Packet Reception at IPv6
  Ptr<Ipv6> ipv6_0 = nodes.Get (0)->GetObject<Ipv6> ();
  Ptr<Ipv6> ipv6_1 = nodes.Get (1)->GetObject<Ipv6> ();

  ipv6_0->TraceConnectWithoutContext ("Rx", MakeCallback (&PacketReceptionTracer));
  ipv6_1->TraceConnectWithoutContext ("Rx", MakeCallback (&PacketReceptionTracer));

  // Install Ping6 Application on n0 destined for n1
  Ipv6Address dstAddr = interfaces.GetAddress (1, 1); // 1: global address of n1

  V6PingHelper ping6 (dstAddr);
  ping6.SetAttribute ("Interval", TimeValue (Seconds (interval)));
  ping6.SetAttribute ("Verbose", BooleanValue (true));
  ping6.SetAttribute ("Count", UintegerValue (nPackets));
  ApplicationContainer apps = ping6.Install (nodes.Get (0));
  apps.Start (Seconds (1.0));
  apps.Stop (Seconds (stopTime - 1));

  Simulator::Stop (Seconds (stopTime));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}