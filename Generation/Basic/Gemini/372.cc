#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

int main (int argc, char *argv[])
{
  // Set the default TCP congestion control algorithm to New Reno
  Config::SetDefault ("ns3::TcpL4Protocol::SocketType", StringValue ("ns3::TcpNewReno"));

  // Create two nodes
  NodeContainer nodes;
  nodes.Create (2);

  // Install the internet stack on the nodes
  InternetStackHelper stack;
  stack.Install (nodes);

  // Configure the point-to-point link properties
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("100Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

  // Install the point-to-point devices on the nodes
  NetDeviceContainer devices = p2p.Install (nodes);

  // Assign IP addresses to the devices
  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = ipv4.Assign (devices);

  // Define a port for TCP communication
  uint16_t port = 9;

  // Create a PacketSink application on the receiver node (node 1)
  PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), port));
  ApplicationContainer sinkApps = sinkHelper.Install (nodes.Get (1)); // Node 1 is the receiver
  sinkApps.Start (Seconds (0.0));
  sinkApps.Stop (Seconds (10.0));

  // Create a BulkSend application on the sender node (node 0)
  // It sends data to the receiver's IP address and port
  BulkSendHelper clientHelper ("ns3::TcpSocketFactory", InetSocketAddress (interfaces.GetAddress (1), port));
  // Set MaxBytes to 0 to send indefinitely until the application stops
  clientHelper.SetAttribute ("MaxBytes", UintegerValue (0)); 
  ApplicationContainer clientApps = clientHelper.Install (nodes.Get (0)); // Node 0 is the sender
  clientApps.Start (Seconds (0.0));
  clientApps.Stop (Seconds (10.0));

  // Set simulation stop time
  Simulator::Stop (Seconds (10.0));

  // Run the simulation
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}