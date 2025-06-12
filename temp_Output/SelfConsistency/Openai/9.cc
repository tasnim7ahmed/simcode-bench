/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Ring Topology using UDP Echo Client-Server in ns-3.
 * Four nodes interconnected in a ring.
 * Each node acts as both client and server at least once.
 * Only one client-server pair active at a time.
 * PointToPoint 5Mbps, 2ms delay.
 * Network: 192.9.39.0/24.
 * Logging and NetAnim support.
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("RingTopologyUdpEchoExample");

int
main (int argc, char *argv[])
{
  Time::SetResolution (Time::NS);
  LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
  LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);

  NS_LOG_INFO ("Create nodes.");
  NodeContainer nodes;
  nodes.Create (4);

  // Keep node coordinates for NetAnim
  std::vector<Vector> nodePositions = {
    Vector ( 50.0, 50.0, 0.0),
    Vector (150.0, 50.0, 0.0),
    Vector (150.0,150.0, 0.0),
    Vector ( 50.0,150.0, 0.0)
  };

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

  // Create the ring: 0<->1, 1<->2, 2<->3, 3<->0
  NetDeviceContainer nd01 = p2p.Install (nodes.Get(0), nodes.Get(1));
  NetDeviceContainer nd12 = p2p.Install (nodes.Get(1), nodes.Get(2));
  NetDeviceContainer nd23 = p2p.Install (nodes.Get(2), nodes.Get(3));
  NetDeviceContainer nd30 = p2p.Install (nodes.Get(3), nodes.Get(0));

  InternetStackHelper stack;
  stack.Install (nodes);

  // Assign IP addresses: total range 192.9.39.0/24
  Ipv4AddressHelper ipv4;
  std::vector<Ipv4InterfaceContainer> interfaces(4);

  ipv4.SetBase ("192.9.39.0", "255.255.255.0");
  interfaces[0] = ipv4.Assign (nd01);
  ipv4.NewNetwork (); // Actually not needed since all in same net, but ensure unique addresses by not colliding
  interfaces[1] = ipv4.Assign (nd12);
  ipv4.NewNetwork ();
  interfaces[2] = ipv4.Assign (nd23);
  ipv4.NewNetwork ();
  interfaces[3] = ipv4.Assign (nd30);

  // We'll use node 0 interface on net 0, node 1 on net1, node 2 on net2, node 3 on net3 for server addresses.

  // UDP Port for echo servers
  uint16_t echoPort = 9;
  double appStart = 1.0; // Application start time
  double gap = 3.0; // App gap time to avoid overlap
  double packetInterval = 0.5;
  uint32_t maxPackets = 4;

  // --- Round 1: node 0 acts as client, node 2 as server ---
  UdpEchoServerHelper echoServer1 (echoPort);
  ApplicationContainer serverApps1 = echoServer1.Install (nodes.Get (2));
  serverApps1.Start (Seconds (appStart));
  serverApps1.Stop (Seconds (appStart + gap));

  UdpEchoClientHelper echoClient1 (interfaces[2].GetAddress (1), echoPort); // node 2's side of 2-3 link
  echoClient1.SetAttribute ("MaxPackets", UintegerValue (maxPackets));
  echoClient1.SetAttribute ("Interval", TimeValue (Seconds (packetInterval)));
  echoClient1.SetAttribute ("PacketSize", UintegerValue (1024));
  ApplicationContainer clientApps1 = echoClient1.Install (nodes.Get (0));
  clientApps1.Start (Seconds (appStart + 0.2));
  clientApps1.Stop (Seconds (appStart + gap));

  // --- Round 2: node 1 acts as client, node 3 as server ---
  UdpEchoServerHelper echoServer2 (echoPort + 1);
  ApplicationContainer serverApps2 = echoServer2.Install (nodes.Get (3));
  serverApps2.Start (Seconds (appStart + gap));
  serverApps2.Stop (Seconds (appStart + 2*gap));

  UdpEchoClientHelper echoClient2 (interfaces[3].GetAddress (1), echoPort + 1); // node 3's side of 3-0 link
  echoClient2.SetAttribute ("MaxPackets", UintegerValue (maxPackets));
  echoClient2.SetAttribute ("Interval", TimeValue (Seconds (packetInterval)));
  echoClient2.SetAttribute ("PacketSize", UintegerValue (1024));
  ApplicationContainer clientApps2 = echoClient2.Install (nodes.Get (1));
  clientApps2.Start (Seconds (appStart + gap + 0.2));
  clientApps2.Stop (Seconds (appStart + 2*gap));

  // --- Round 3: node 2 acts as client, node 0 as server ---
  UdpEchoServerHelper echoServer3 (echoPort + 2);
  ApplicationContainer serverApps3 = echoServer3.Install (nodes.Get (0));
  serverApps3.Start (Seconds (appStart + 2*gap));
  serverApps3.Stop (Seconds (appStart + 3*gap));

  UdpEchoClientHelper echoClient3 (interfaces[0].GetAddress (0), echoPort + 2); // node 0's side of 0-1 link
  echoClient3.SetAttribute ("MaxPackets", UintegerValue (maxPackets));
  echoClient3.SetAttribute ("Interval", TimeValue (Seconds (packetInterval)));
  echoClient3.SetAttribute ("PacketSize", UintegerValue (1024));
  ApplicationContainer clientApps3 = echoClient3.Install (nodes.Get (2));
  clientApps3.Start (Seconds (appStart + 2*gap + 0.2));
  clientApps3.Stop (Seconds (appStart + 3*gap));

  // --- Round 4: node 3 acts as client, node 1 as server ---
  UdpEchoServerHelper echoServer4 (echoPort + 3);
  ApplicationContainer serverApps4 = echoServer4.Install (nodes.Get (1));
  serverApps4.Start (Seconds (appStart + 3*gap));
  serverApps4.Stop (Seconds (appStart + 4*gap));

  UdpEchoClientHelper echoClient4 (interfaces[1].GetAddress (0), echoPort + 3); // node 1's side of 1-2 link
  echoClient4.SetAttribute ("MaxPackets", UintegerValue (maxPackets));
  echoClient4.SetAttribute ("Interval", TimeValue (Seconds (packetInterval)));
  echoClient4.SetAttribute ("PacketSize", UintegerValue (1024));
  ApplicationContainer clientApps4 = echoClient4.Install (nodes.Get (3));
  clientApps4.Start (Seconds (appStart + 3*gap + 0.2));
  clientApps4.Stop (Seconds (appStart + 4*gap));

  // NetAnim setup
  AnimationInterface anim ("ring-topology.xml");
  for (uint32_t i = 0; i < nodes.GetN (); ++i)
    {
      anim.SetConstantPosition (nodes.Get (i), nodePositions[i].x, nodePositions[i].y);
    }

  anim.SetMaxPktsPerTraceFile (50000);
  anim.EnablePacketMetadata (true);
  anim.EnableIpv4RouteTracking ("ring-routes.xml", Seconds (0), Seconds (appStart + 4*gap), Seconds (0.25));

  NS_LOG_INFO ("Run Simulation.");
  Simulator::Stop (Seconds (appStart + 4*gap + 1.0));
  Simulator::Run ();
  Simulator::Destroy ();
  NS_LOG_INFO ("Done.");

  return 0;
}