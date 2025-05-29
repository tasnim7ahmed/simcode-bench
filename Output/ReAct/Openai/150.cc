#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int
main (int argc, char *argv[])
{
  Time::SetResolution (Time::NS);

  NodeContainer nodes;
  nodes.Create (4);

  // Create point-to-point links between consecutive nodes
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

  // Link 0-1
  NodeContainer n0n1 = NodeContainer (nodes.Get (0), nodes.Get (1));
  NetDeviceContainer d0d1 = p2p.Install (n0n1);

  // Link 1-2
  NodeContainer n1n2 = NodeContainer (nodes.Get (1), nodes.Get (2));
  NetDeviceContainer d1d2 = p2p.Install (n1n2);

  // Link 2-3
  NodeContainer n2n3 = NodeContainer (nodes.Get (2), nodes.Get (3));
  NetDeviceContainer d2d3 = p2p.Install (n2n3);

  // Install Internet stack
  InternetStackHelper internet;
  internet.Install (nodes);

  // Assign IP addresses
  Ipv4AddressHelper address;

  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i0i1 = address.Assign (d0d1);

  address.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer i1i2 = address.Assign (d1d2);

  address.SetBase ("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer i2i3 = address.Assign (d2d3);

  // Enable static routing
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // Set up UDP traffic: node 0 sends to node 3
  uint16_t port = 4000;

  // Create UDP server (sink) on node 3
  UdpServerHelper udpServer (port);
  ApplicationContainer serverApps = udpServer.Install (nodes.Get (3));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  // Create UDP client on node 0, sending to node 3's interface
  UdpClientHelper udpClient (i2i3.GetAddress (1), port); // Node 3's address on link 2-3
  udpClient.SetAttribute ("MaxPackets", UintegerValue (100000));
  udpClient.SetAttribute ("Interval", TimeValue (MilliSeconds (50)));
  udpClient.SetAttribute ("PacketSize", UintegerValue (1024));
  ApplicationContainer clientApps = udpClient.Install (nodes.Get (0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  // Enable PCAP tracing
  p2p.EnablePcapAll ("line-topology");

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}