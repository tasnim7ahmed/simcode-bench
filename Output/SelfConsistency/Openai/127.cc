/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Simulation: OSPF (Link State Routing) with 4 routers in NS-3
 * Topology:
 *    n0---n1---n2---n3
 *
 * Traffic: UDP from n0 to n3
 * PCAP tracing is enabled.
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/csma-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("OspfLinkStateExample");

int
main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  // Enable OSPF globally
  Config::SetDefault ("ns3::Ipv4GlobalRouting::RespondToInterfaceEvents", BooleanValue (true));
  Config::SetDefault ("ns3::Ipv4RoutingHelper::Ipv4RoutingProtocol", StringValue ("ns3::OspfRoutingHelper"));

  // Create four router nodes
  NodeContainer routers;
  routers.Create (4);

  // Install Internet stack with OSPF
  OspfRoutingHelper ospfRouting;
  InternetStackHelper stack;
  stack.SetRoutingHelper (ospfRouting);
  stack.Install (routers);

  // Create point-to-point links between nodes to form a line topology
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

  // n0 <-> n1
  NetDeviceContainer d01 = p2p.Install (routers.Get (0), routers.Get (1));
  // n1 <-> n2
  NetDeviceContainer d12 = p2p.Install (routers.Get (1), routers.Get (2));
  // n2 <-> n3
  NetDeviceContainer d23 = p2p.Install (routers.Get (2), routers.Get (3));

  // Assign IP addresses to each link
  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i01 = ipv4.Assign (d01);

  ipv4.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer i12 = ipv4.Assign (d12);

  ipv4.SetBase ("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer i23 = ipv4.Assign (d23);

  // Allow routes to be discovered
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // Install applications: UDP echo from n0 to n3
  uint16_t echoPort = 9;

  // UDP Echo Server on n3
  UdpEchoServerHelper echoServer (echoPort);
  ApplicationContainer serverApps = echoServer.Install (routers.Get (3));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  // UDP Echo Client on n0
  UdpEchoClientHelper echoClient (i23.GetAddress (1), echoPort); // Address of n3 on n2-n3 link
  echoClient.SetAttribute ("MaxPackets", UintegerValue (5));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (64));
  ApplicationContainer clientApps = echoClient.Install (routers.Get (0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  // Enable PCAP tracing on all devices
  p2p.EnablePcapAll ("ospf-link-state");

  // Enable routing table logging
  Ptr<OutputStreamWrapper> routingStream = Create<OutputStreamWrapper> (&std::cout);
  for (uint32_t i = 0; i < routers.GetN (); ++i)
    {
      Ptr<Ipv4> ipv4 = routers.Get (i)->GetObject<Ipv4> ();
      Ipv4RoutingHelper::PrintRoutingTableAt (Seconds (2.5), ipv4, routingStream);
      Ipv4RoutingHelper::PrintRoutingTableAt (Seconds (9.5), ipv4, routingStream);
    }

  Simulator::Stop (Seconds (11.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}