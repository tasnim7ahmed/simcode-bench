#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("MixedTopologySimulation");

void
RxTraceCallback(Ptr<OutputStreamWrapper> stream, Ptr<const Packet> packet, const Address &from)
{
  *stream->GetStream () << Simulator::Now ().GetSeconds () << " " << packet->GetSize () << " from " << InetSocketAddress::ConvertFrom (from).GetIpv4 () << std::endl;
}

void
QueueTraceCallback(Ptr<OutputStreamWrapper> stream, uint32_t oldSize, uint32_t newSize)
{
  *stream->GetStream () << Simulator::Now ().GetSeconds () << " " << oldSize << " " << newSize << std::endl;
}

int
main (int argc, char *argv[])
{
  Time::SetResolution (Time::NS);

  NodeContainer n0n2, n1n2, n5n6, csmaNodes;
  Ptr<Node> n0 = CreateObject<Node> ();
  Ptr<Node> n1 = CreateObject<Node> ();
  Ptr<Node> n2 = CreateObject<Node> ();
  Ptr<Node> n3 = CreateObject<Node> ();
  Ptr<Node> n4 = CreateObject<Node> ();
  Ptr<Node> n5 = CreateObject<Node> ();
  Ptr<Node> n6 = CreateObject<Node> ();

  n0n2.Add (n0);
  n0n2.Add (n2);

  n1n2.Add (n1);
  n1n2.Add (n2);

  n5n6.Add (n5);
  n5n6.Add (n6);

  csmaNodes.Add (n2);
  csmaNodes.Add (n3);
  csmaNodes.Add (n4);
  csmaNodes.Add (n5);

  // Point-to-Point configurations
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

  // Install devices
  NetDeviceContainer d0n2 = p2p.Install (n0n2);
  NetDeviceContainer d1n2 = p2p.Install (n1n2);
  NetDeviceContainer d5n6 = p2p.Install (n5n6);

  // CSMA configuration
  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", StringValue ("100Mbps"));
  csma.SetChannelAttribute ("Delay", TimeValue (NanoSeconds (6560)));

  NetDeviceContainer csmaDevs = csma.Install (csmaNodes);

  // Internet stack
  InternetStackHelper internet;
  internet.Install (n0);
  internet.Install (n1);
  internet.Install (n2);
  internet.Install (n3);
  internet.Install (n4);
  internet.Install (n5);
  internet.Install (n6);

  // Assign IP addresses
  Ipv4AddressHelper ipv4;

  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer if0n2 = ipv4.Assign (d0n2);

  ipv4.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer if1n2 = ipv4.Assign (d1n2);

  ipv4.SetBase ("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer ifCsma = ipv4.Assign (csmaDevs);

  ipv4.SetBase ("10.1.4.0", "255.255.255.0");
  Ipv4InterfaceContainer if5n6 = ipv4.Assign (d5n6);

  // Set up routing
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // Configure the OnOff application: CBR/UDP from n0 to n6
  uint16_t port = 4000;
  Address sinkAddress (InetSocketAddress (if5n6.GetAddress (1), port));

  PacketSinkHelper sinkHelper ("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), port));
  ApplicationContainer sinkApp = sinkHelper.Install (n6);
  sinkApp.Start (Seconds (0.0));
  sinkApp.Stop (Seconds (15.0));

  OnOffHelper onoff ("ns3::UdpSocketFactory", sinkAddress);
  onoff.SetAttribute ("PacketSize", UintegerValue (50));
  onoff.SetAttribute ("DataRate", StringValue ("300bps"));
  onoff.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
  onoff.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));

  ApplicationContainer app = onoff.Install (n0);
  app.Start (Seconds (1.0));
  app.Stop (Seconds (14.0));

  // Trace queues on all point-to-point and CSMA devices
  AsciiTraceHelper asciiTraceHelper;
  Ptr<OutputStreamWrapper> queueStream = asciiTraceHelper.CreateFileStream ("queue-trace.txt");
  Ptr<OutputStreamWrapper> rxStream = asciiTraceHelper.CreateFileStream ("rx-trace.txt");

  // Trace packet receptions at n6
  n6->GetApplication (0)->GetObject<PacketSink> ()->TraceConnectWithoutContext (
    "RxWithAddresses", MakeBoundCallback (&RxTraceCallback, rxStream));

  // Trace TxQueues for p2p and csma devices
  // d0n2.Get (0): n0's p2p device
  // d0n2.Get (1): n2's p2p device
  // d1n2.Get (0): n1's p2p device
  // d1n2.Get (1): n2's p2p device
  // d5n6.Get (0): n5's p2p device
  // d5n6.Get (1): n6's p2p device
  for (uint32_t i = 0; i < 2; ++i)
    {
      Ptr<PointToPointNetDevice> dev = DynamicCast<PointToPointNetDevice> (d0n2.Get (i));
      dev->GetQueue ()->TraceConnectWithoutContext ("PacketsInQueue", MakeBoundCallback (&QueueTraceCallback, queueStream));
      dev = DynamicCast<PointToPointNetDevice> (d1n2.Get (i));
      dev->GetQueue ()->TraceConnectWithoutContext ("PacketsInQueue", MakeBoundCallback (&QueueTraceCallback, queueStream));
      dev = DynamicCast<PointToPointNetDevice> (d5n6.Get (i));
      dev->GetQueue ()->TraceConnectWithoutContext ("PacketsInQueue", MakeBoundCallback (&QueueTraceCallback, queueStream));
    }
  for (uint32_t i = 0; i < csmaDevs.GetN (); ++i)
    {
      Ptr<CsmaNetDevice> csmaDev = DynamicCast<CsmaNetDevice> (csmaDevs.Get (i));
      csmaDev->GetQueue ()->TraceConnectWithoutContext ("PacketsInQueue", MakeBoundCallback (&QueueTraceCallback, queueStream));
    }

  // Enable pcap tracing for all channels
  p2p.EnablePcapAll ("p2p");
  csma.EnablePcapAll ("csma");

  Simulator::Stop (Seconds (15.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}