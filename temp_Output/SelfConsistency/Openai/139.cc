/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("SixNodeUdpEcho");

int
main (int argc, char *argv[])
{
  // Enable logging
  LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
  LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);

  // Create 6 nodes
  NodeContainer nodes;
  nodes.Create (6);

  // Point-to-point helpers for each link
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

  // Create point-to-point links between n0-n1, n1-n2, n2-n3, n3-n4, n4-n5
  NetDeviceContainer dev01 = p2p.Install (nodes.Get (0), nodes.Get (1));
  NetDeviceContainer dev12 = p2p.Install (nodes.Get (1), nodes.Get (2));
  NetDeviceContainer dev23 = p2p.Install (nodes.Get (2), nodes.Get (3));
  NetDeviceContainer dev34 = p2p.Install (nodes.Get (3), nodes.Get (4));
  NetDeviceContainer dev45 = p2p.Install (nodes.Get (4), nodes.Get (5));

  // Install the Internet stack
  InternetStackHelper stack;
  stack.Install (nodes);

  // Assign IP addresses for each link
  Ipv4AddressHelper address;

  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer if01 = address.Assign (dev01);

  address.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer if12 = address.Assign (dev12);

  address.SetBase ("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer if23 = address.Assign (dev23);

  address.SetBase ("10.1.4.0", "255.255.255.0");
  Ipv4InterfaceContainer if34 = address.Assign (dev34);

  address.SetBase ("10.1.5.0", "255.255.255.0");
  Ipv4InterfaceContainer if45 = address.Assign (dev45);

  // Enable routing
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // UDP Echo Server on node 5
  uint16_t port = 9;
  UdpEchoServerHelper echoServer (port);

  ApplicationContainer serverApps = echoServer.Install (nodes.Get (5));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  // UDP Echo Client on node 0, destination is the IP address of node 5 (on its interface to node 4)
  UdpEchoClientHelper echoClient (if45.GetAddress (1), port);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (5));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));

  ApplicationContainer clientApps = echoClient.Install (nodes.Get (0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  // Enable pcap tracing on all devices
  p2p.EnablePcapAll ("six-node-udp-echo");

  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}