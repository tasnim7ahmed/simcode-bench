#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("PacketLossDemo");

void
QueueDiscDropLogger(Ptr<const QueueDiscItem> item)
{
  NS_LOG_UNCOND ("Packet dropped: " << Simulator::Now ().GetSeconds () << "s " 
                  << item->GetPacket ()->GetUid ());
}

int
main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  NodeContainer nodes;
  nodes.Create (2);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("1Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer devices = pointToPoint.Install (nodes);

  TrafficControlHelper tch;
  uint16_t handle = tch.SetRootQueueDisc ("ns3::PfifoFastQueueDisc", "MaxSize", QueueSizeValue (QueueSize ("5p")));
  QueueDiscContainer qdiscs = tch.Install (devices);

  qdiscs.Get(0)->TraceConnectWithoutContext ("Drop", MakeCallback (&QueueDiscDropLogger));

  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  uint16_t port = 9999;

  UdpServerHelper server (port);
  ApplicationContainer serverApp = server.Install (nodes.Get (1));
  serverApp.Start (Seconds (0.0));
  serverApp.Stop (Seconds (5.0));

  // Intentionally overload the queue: set high data rate and packet rate
  uint32_t packetSize = 1024;
  uint32_t numPackets = 2000;
  double interval = 0.001; // 1ms between packets (1000 packets/sec), will induce loss.

  UdpClientHelper client (interfaces.GetAddress (1), port);
  client.SetAttribute ("MaxPackets", UintegerValue (numPackets));
  client.SetAttribute ("Interval", TimeValue (Seconds (interval)));
  client.SetAttribute ("PacketSize", UintegerValue (packetSize));

  ApplicationContainer clientApp = client.Install (nodes.Get (0));
  clientApp.Start (Seconds (0.5));
  clientApp.Stop (Seconds (5.0));

  AnimationInterface anim ("packet-loss-demo.xml");
  anim.SetConstantPosition (nodes.Get (0), 10, 20);
  anim.SetConstantPosition (nodes.Get (1), 40, 20);

  Simulator::Stop (Seconds (5.1));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}