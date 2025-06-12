#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("RingTopologyTcpNetAnim");

int
main (int argc, char *argv[])
{
  Time::SetResolution (Time::NS);

  uint32_t numNodes = 4;
  NodeContainer nodes;
  nodes.Create (numNodes);

  // Create the 4 links to create a ring: 0-1, 1-2, 2-3, 3-0
  std::vector<PointToPointHelper> p2pLinks (4);
  NetDeviceContainer devices[4];
  Ipv4InterfaceContainer interfaces[4];

  // P2P configuration
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("1ms"));

  // Manually create links for the ring
  devices[0] = p2p.Install (nodes.Get (0), nodes.Get (1));
  devices[1] = p2p.Install (nodes.Get (1), nodes.Get (2));
  devices[2] = p2p.Install (nodes.Get (2), nodes.Get (3));
  devices[3] = p2p.Install (nodes.Get (3), nodes.Get (0));

  // Install Internet stack
  InternetStackHelper stack;
  stack.Install (nodes);

  // Assign IP addresses to the links
  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  interfaces[0] = address.Assign (devices[0]);
  address.SetBase ("10.1.2.0", "255.255.255.0");
  interfaces[1] = address.Assign (devices[1]);
  address.SetBase ("10.1.3.0", "255.255.255.0");
  interfaces[2] = address.Assign (devices[2]);
  address.SetBase ("10.1.4.0", "255.255.255.0");
  interfaces[3] = address.Assign (devices[3]);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // Set positions for NetAnim
  Ptr<ConstantPositionMobilityModel> m0 = CreateObject<ConstantPositionMobilityModel> ();
  Ptr<ConstantPositionMobilityModel> m1 = CreateObject<ConstantPositionMobilityModel> ();
  Ptr<ConstantPositionMobilityModel> m2 = CreateObject<ConstantPositionMobilityModel> ();
  Ptr<ConstantPositionMobilityModel> m3 = CreateObject<ConstantPositionMobilityModel> ();
  nodes.Get (0)->AggregateObject (m0);
  nodes.Get (1)->AggregateObject (m1);
  nodes.Get (2)->AggregateObject (m2);
  nodes.Get (3)->AggregateObject (m3);

  m0->SetPosition (Vector (30.0, 50.0, 0.0));
  m1->SetPosition (Vector (70.0, 70.0, 0.0));
  m2->SetPosition (Vector (110.0, 50.0, 0.0));
  m3->SetPosition (Vector (70.0, 10.0, 0.0));

  // Install Applications: TCP server on Node 2, TCP client on Node 0

  // Server setup (Node 2)
  uint16_t port = 50000;
  Address serverAddress (InetSocketAddress (interfaces[2].GetAddress (1), port));
  PacketSinkHelper packetSinkHelper ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), port));
  ApplicationContainer serverApp = packetSinkHelper.Install (nodes.Get (2));
  serverApp.Start (Seconds (0.0));
  serverApp.Stop (Seconds (15.0));

  // TCP client on Node 0, connects to Node 2, persistent connection
  OnOffHelper client ("ns3::TcpSocketFactory", serverAddress);
  client.SetAttribute ("DataRate", StringValue ("1Mbps"));
  client.SetAttribute ("PacketSize", UintegerValue (512));
  client.SetAttribute ("MaxBytes", UintegerValue (0)); // persistent, unlimited bytes
  client.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
  client.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));

  ApplicationContainer clientApp = client.Install (nodes.Get (0));
  clientApp.Start (Seconds (1.0));
  clientApp.Stop (Seconds (15.0));

  // Enable packet tracing (ASCII and PCAP)
  AsciiTraceHelper ascii;
  p2p.EnableAsciiAll (ascii.CreateFileStream ("ring-tcp.tr"));
  p2p.EnablePcapAll ("ring-tcp");

  // Set up NetAnim
  AnimationInterface anim ("ring-topology.xml");
  anim.SetConstantPosition (nodes.Get (0), 30.0, 50.0);
  anim.SetConstantPosition (nodes.Get (1), 70.0, 70.0);
  anim.SetConstantPosition (nodes.Get (2), 110.0, 50.0);
  anim.SetConstantPosition (nodes.Get (3), 70.0, 10.0);

  anim.UpdateNodeDescription (0, "TCP Client");
  anim.UpdateNodeDescription (1, "Forwarder1");
  anim.UpdateNodeDescription (2, "TCP Server");
  anim.UpdateNodeDescription (3, "Forwarder2");
  anim.UpdateNodeColor (0, 0, 255, 0);   // Green
  anim.UpdateNodeColor (2, 255, 0, 0);   // Red

  // Trace packet transmissions
  Config::Connect ("/NodeList/*/DeviceList/*/$ns3::PointToPointNetDevice/PhyTxEnd",
                   MakeCallback ([] (Ptr<const Packet> packet) {
                     NS_LOG_INFO ("Packet transmitted: " << packet->GetSize () << " bytes");
                   }));

  Simulator::Stop (Seconds (15.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}