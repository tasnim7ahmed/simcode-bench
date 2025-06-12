/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Network topology:
 * n0 ---- p2p ---- n1
 *
 * BulkSendApplication runs from n0 to n1 over TCP NewReno.
 * NetAnim animation file is generated as "wired-p2p-tcpnewreno.xml"
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WiredP2PNewRenoExample");

int 
main (int argc, char *argv[])
{
  Time::SetResolution (Time::NS);
  LogComponentEnable ("BulkSendApplication", LOG_LEVEL_INFO);
  LogComponentEnable ("PacketSink", LOG_LEVEL_INFO);

  // Create two nodes
  NodeContainer nodes;
  nodes.Create (2);

  // Configure PointToPoint link
  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer devices;
  devices = pointToPoint.Install (nodes);

  // Install the Internet stack
  InternetStackHelper stack;
  stack.Install (nodes);

  // Assign IP addresses
  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  // Set TCP variant to NewReno
  Config::SetDefault ("ns3::TcpL4Protocol::SocketType", StringValue ("ns3::TcpNewReno"));

  // Install PacketSink on node 1
  uint16_t sinkPort = 8080;
  Address sinkAddress (InetSocketAddress (interfaces.GetAddress (1), sinkPort));
  PacketSinkHelper packetSinkHelper ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), sinkPort));
  ApplicationContainer sinkApp = packetSinkHelper.Install (nodes.Get (1));
  sinkApp.Start (Seconds (0.));
  sinkApp.Stop (Seconds (10.0));

  // Install BulkSendApplication on node 0
  BulkSendHelper bulkSender ("ns3::TcpSocketFactory", sinkAddress);
  // Send unlimited data
  bulkSender.SetAttribute ("MaxBytes", UintegerValue (0));
  bulkSender.SetAttribute ("SendSize", UintegerValue (1024)); // Send in 1KB chunks

  ApplicationContainer bulkApp = bulkSender.Install (nodes.Get (0));
  bulkApp.Start (Seconds (1.0));
  bulkApp.Stop (Seconds (10.0));

  // Enable pcap tracing to visualize packet exchanges
  pointToPoint.EnablePcapAll ("wired-p2p-tcpnewreno", true);

  // NetAnim setup
  AnimationInterface anim ("wired-p2p-tcpnewreno.xml");
  anim.SetConstantPosition (nodes.Get (0), 10.0, 20.0);
  anim.SetConstantPosition (nodes.Get (1), 60.0, 20.0);

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}