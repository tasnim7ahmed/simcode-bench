#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int
main (int argc, char *argv[])
{
  Time::SetResolution (Time::NS);

  // Create 4 nodes
  NodeContainer nodes;
  nodes.Create (4);

  // Set up the CSMA channel (bus topology)
  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", StringValue ("100Mbps"));
  csma.SetChannelAttribute ("Delay", TimeValue (NanoSeconds (6560)));

  // Install CSMA on the nodes
  NetDeviceContainer devices;
  devices = csma.Install (nodes);

  // Install and assign IP addresses
  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");

  Ipv4InterfaceContainer interfaces;
  interfaces = address.Assign (devices);

  // Set up UDP Echo Server on node 3
  uint16_t echoPort = 9;
  UdpEchoServerHelper echoServer (echoPort);

  ApplicationContainer serverApps = echoServer.Install (nodes.Get (3));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  // Set up UDP Echo Client on node 0 (sending to node 3)
  UdpEchoClientHelper echoClient (interfaces.GetAddress (3), echoPort);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (10));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));

  ApplicationContainer clientApps = echoClient.Install (nodes.Get (0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  // Enable PCAP tracing
  csma.EnablePcapAll ("bus-topology");

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}