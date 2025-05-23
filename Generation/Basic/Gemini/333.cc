#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

// Define NS_LOG for this example if needed for debugging,
// but problem description implies no extra output.
// NS_LOG_COMPONENT_DEFINE ("TcpClientServerExample");

int main (int argc, char *argv[])
{
  // Set default parameters for TCP
  // Config::SetDefault ("ns3::TcpL4Protocol::SocketType", StringValue ("ns3::TcpNewReno"));

  // Create nodes
  NodeContainer nodes;
  nodes.Create (2); // Node 0 (client), Node 1 (server)

  // Configure Point-to-Point link
  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer devices;
  devices = pointToPoint.Install (nodes);

  // Install Internet Stack
  InternetStackHelper internet;
  internet.Install (nodes);

  // Assign IP addresses
  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = ipv4.Assign (devices);

  // Server (Node 1) setup
  uint16_t port = 8080;
  Address serverAddress (InetSocketAddress (Ipv4Address::GetAny (), port)); // Server listens on any IP

  PacketSinkHelper packetSinkHelper ("ns3::TcpSocketFactory", serverAddress);
  ApplicationContainer serverApps = packetSinkHelper.Install (nodes.Get (1)); // Install on Node 1
  serverApps.Start (Seconds (1.0)); // Server starts accepting connections at 1 second
  serverApps.Stop (Seconds (10.0)); // Server stops accepting connections at 10 seconds

  // Client (Node 0) setup
  OnOffHelper onOffClient ("ns3::TcpSocketFactory", Address ()); // Address will be set later
  
  // Send 10 packets, each of 1024 bytes.
  // To achieve an interval of 1 second per 1024-byte packet,
  // the data rate should be 1024 bytes/second = 8192 bits/second.
  onOffClient.SetAttribute ("PacketSize", UintegerValue (1024));
  onOffClient.SetAttribute ("DataRate", DataRateValue ("8192bps")); // 1024 Bytes/s
  onOffClient.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1.0]")); // Keep On for 1 sec
  onOffClient.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0.0]")); // No off time; it's effectively always trying to send
  onOffClient.SetAttribute ("MaxBytes", UintegerValue (10 * 1024)); // Send a total of 10 * 1024 bytes (10 packets)

  ApplicationContainer clientApps = onOffClient.Install (nodes.Get (0)); // Install on Node 0
  clientApps.Start (Seconds (2.0)); // Client starts sending at 2 seconds
  // The client needs to send 10 packets, one every second, starting at 2s.
  // Packet 1: 2s, Packet 2: 3s, ..., Packet 10: 11s.
  // To ensure all 10 packets are sent, the client application must run until at least 11 seconds.
  clientApps.Stop (Seconds (11.0)); // Client stops after sending all 10 packets

  // Set the remote address for the client application
  AddressValue remoteAddress (InetSocketAddress (interfaces.GetAddress (1), port));
  clientApps.Get (0)->SetAttribute ("Remote", remoteAddress);

  // Simulation setup
  Simulator::Stop (Seconds (12.0)); // Overall simulation stops after all applications have completed
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}