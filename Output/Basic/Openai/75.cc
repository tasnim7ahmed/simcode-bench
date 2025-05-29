#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/queue-disc-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("MixedP2pCsmaExample");

void
PacketReceiveCallback (Ptr<OutputStreamWrapper> stream, Ptr<const Packet> packet, const Address &)
{
  *stream->GetStream () << Simulator::Now ().GetSeconds () << " " << packet->GetSize () << std::endl;
}

int
main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  NodeContainer p2p1Nodes;
  p2p1Nodes.Create (3); // n0, n1, n2

  NodeContainer csmaNodes;
  // n2, n3, n4, n5
  csmaNodes.Add (p2p1Nodes.Get (2));
  csmaNodes.Create (3); // n3, n4, n5

  NodeContainer p2p2Nodes;
  p2p2Nodes.Add (csmaNodes.Get (3)); // n5
  p2p2Nodes.Create (1); // n6

  // Point-to-Point links
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer p2pDevices_n0_n2 = p2p.Install (NodeContainer (p2p1Nodes.Get (0), p2p1Nodes.Get (2))); // n0-n2
  NetDeviceContainer p2pDevices_n1_n2 = p2p.Install (NodeContainer (p2p1Nodes.Get (1), p2p1Nodes.Get (2))); // n1-n2

  // CSMA (Ethernet bus)
  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", StringValue ("100Mbps"));
  csma.SetChannelAttribute ("Delay", TimeValue (NanoSeconds (6560)));

  NetDeviceContainer csmaDevices = csma.Install (csmaNodes);

  // Point-to-Point link between n5 (csmaNodes.Get(3)) and n6 (p2p2Nodes.Get(1))
  NetDeviceContainer p2pDevices_n5_n6 = p2p.Install (NodeContainer (csmaNodes.Get (3), p2p2Nodes.Get (1)));

  // Stack
  InternetStackHelper stack;
  stack.Install (p2p1Nodes);
  // n2 already installed. Only n3, n4, n5
  stack.Install (csmaNodes.Get (1));
  stack.Install (csmaNodes.Get (2));
  stack.Install (csmaNodes.Get (3));
  stack.Install (p2p2Nodes.Get (1));

  // IP address assignment
  Ipv4AddressHelper address;

  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces_n0_n2 = address.Assign (p2pDevices_n0_n2);

  address.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces_n1_n2 = address.Assign (p2pDevices_n1_n2);

  address.SetBase ("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces_csma = address.Assign (csmaDevices);

  address.SetBase ("10.1.4.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces_n5_n6 = address.Assign (p2pDevices_n5_n6);

  // UDP Server (sink) on n6
  uint16_t port = 9999;
  UdpServerHelper server (port);
  ApplicationContainer serverApp = server.Install (p2p2Nodes.Get (1));
  serverApp.Start (Seconds (0.0));
  serverApp.Stop (Seconds (20.0));

  // UDP Client (CBR) on n0 -> n6
  UdpClientHelper client (interfaces_n5_n6.GetAddress (1), port);
  client.SetAttribute ("MaxPackets", UintegerValue (1000000));
  client.SetAttribute ("Interval", TimeValue (Seconds ((210.0 * 8) / 448000.0))); // Packet size/data rate
  client.SetAttribute ("PacketSize", UintegerValue (210));

  ApplicationContainer clientApp = client.Install (p2p1Nodes.Get (0));
  clientApp.Start (Seconds (1.0));
  clientApp.Stop (Seconds (20.0));

  // Tracing: PCAP for all links
  p2p.EnablePcapAll ("mixed-p2p-csma");
  csma.EnablePcapAll ("mixed-p2p-csma-csma");

  // Tracing: Queue tracing
  // Point-to-Point queue
  Ptr<Queue<Packet>> queue_n0 = DynamicCast<PointToPointNetDevice> (p2pDevices_n0_n2.Get (0))->GetQueue ();
  queue_n0->TraceConnect ("PacketsInQueue", "mixed-p2p-csma-queue-n0.tr",
      MakeBoundCallback (&QueueDisc::TraceEnqueue));

  Ptr<Queue<Packet>> queue_n2 = DynamicCast<PointToPointNetDevice> (p2pDevices_n0_n2.Get (1))->GetQueue ();
  queue_n2->TraceConnect ("PacketsInQueue", "mixed-p2p-csma-queue-n2.tr",
      MakeBoundCallback (&QueueDisc::TraceEnqueue));

  // CSMA queue (for example, n2)
  Ptr<Queue<Packet>> csmaQueue = DynamicCast<CsmaNetDevice> (csmaDevices.Get (0))->GetQueue ();
  csmaQueue->TraceConnect ("PacketsInQueue", "mixed-p2p-csma-csma-queue-n2.tr",
      MakeBoundCallback (&QueueDisc::TraceEnqueue));

  // Packet reception tracing (n6)
  AsciiTraceHelper ascii;
  Ptr<OutputStreamWrapper> stream = ascii.CreateFileStream ("mixed-p2p-csma-rx.log");
  Ptr<NetDevice> n6Dev = p2pDevices_n5_n6.Get (1);
  n6Dev->TraceConnectWithoutContext ("PhyRxEnd", MakeBoundCallback (&PacketReceiveCallback, stream));

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  Simulator::Stop (Seconds (20.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}