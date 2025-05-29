/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("CsmaThreeNodeUdpRelayExample");

// Callback function to log received packets at the server
void
RxPacketLogger (Ptr<const Packet> packet, const Address &from)
{
  NS_LOG_UNCOND ("Server received packet of size " << packet->GetSize () << " bytes from " << InetSocketAddress::ConvertFrom (from).GetIpv4 ());
}

int
main (int argc, char *argv[])
{
  LogComponentEnable ("CsmaThreeNodeUdpRelayExample", LOG_LEVEL_INFO);

  // Step 1: Create nodes
  NodeContainer nodes;
  nodes.Create (3); // n0: client, n1: relay, n2: server

  // Step 2: Create CSMA channel
  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", StringValue ("100Mbps"));
  csma.SetChannelAttribute ("Delay", TimeValue (NanoSeconds (6560)));

  NetDeviceContainer devices = csma.Install (nodes);

  // Step 3: Install Internet stack
  InternetStackHelper stack;
  stack.Install (nodes);

  // Step 4: Assign IP addresses
  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  // Step 5: Set up the UDP server application on Node 2 (n2)
  uint16_t serverPort = 4000;
  UdpServerHelper serverHelper (serverPort);
  ApplicationContainer serverApps = serverHelper.Install (nodes.Get (2));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  // Connect to the server's Rx trace to log received packets
  Ptr<UdpServer> serverApp = DynamicCast<UdpServer> (serverApps.Get (0));
  serverApp->TraceConnectWithoutContext ("Rx", MakeCallback (&RxPacketLogger));

  // Step 6: Set up the UDP client application on Node 0 (n0)
  // The client should send to the server's address (interfaces.GetAddress (2))
  UdpClientHelper clientHelper (interfaces.GetAddress (2), serverPort);
  clientHelper.SetAttribute ("MaxPackets", UintegerValue (10));
  clientHelper.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  clientHelper.SetAttribute ("PacketSize", UintegerValue (512));

  ApplicationContainer clientApps = clientHelper.Install (nodes.Get (0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (9.0));

  // Step 7: Enable pcap tracing (optional)
  csma.EnablePcapAll ("csma-three-node-udp-relay", false);

  // Step 8: Run simulation
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}