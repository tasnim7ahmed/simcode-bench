#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

int main ()
{
  // Create two nodes
  ns3::NodeContainer nodes;
  nodes.Create (2);

  // Configure and install point-to-point devices
  ns3::PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", ns3::StringValue ("100Mbps"));
  p2p.SetChannelAttribute ("Delay", ns3::StringValue ("2ms"));

  ns3::NetDeviceContainer devices;
  devices = p2p.Install (nodes);

  // Install the Internet stack on the nodes
  ns3::InternetStackHelper stack;
  stack.Install (nodes);

  // Assign IP addresses to the devices
  ns3::Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  ns3::Ipv4InterfaceContainer interfaces = address.Assign (devices);

  // Set up the PacketSink (receiver) application on Node 1
  uint16_t port = 9;
  ns3::Address sinkLocalAddress (ns3::InetSocketAddress (ns3::Ipv4Address::GetAny (), port));
  ns3::PacketSinkHelper packetSinkHelper ("ns3::TcpSocketFactory", sinkLocalAddress);
  ns3::ApplicationContainer sinkApps = packetSinkHelper.Install (nodes.Get (1)); // Node 1 is receiver
  sinkApps.Start (ns3::Seconds (0.0));
  sinkApps.Stop (ns3::Seconds (11.0)); // Run a bit longer than client to ensure all data is received

  // Set up the OnOffApplication (sender) on Node 0
  ns3::OnOffHelper onoffHelper ("ns3::TcpSocketFactory", ns3::Address ()); // Address will be set later
  onoffHelper.SetAttribute ("OnTime", ns3::StringValue ("ns3::ConstantRandomVariable[Constant=10.0]")); // On for 10 seconds
  onoffHelper.SetAttribute ("OffTime", ns3::StringValue ("ns3::ConstantRandomVariable[Constant=0.0]")); // Never off
  onoffHelper.SetAttribute ("DataRate", ns3::DataRateValue ("8.192Mbps")); // 1024 bytes/ms = 8.192 Mbps
  onoffHelper.SetAttribute ("PacketSize", ns3::UintegerValue (1024)); // Packet size 1024 bytes

  // Set the remote address (Node 1's IP and port) for the OnOffApplication
  ns3::AddressValue remoteAddress (ns3::InetSocketAddress (interfaces.GetAddress (1), port));
  onoffHelper.SetAttribute ("Remote", remoteAddress);

  ns3::ApplicationContainer clientApps = onoffHelper.Install (nodes.Get (0)); // Node 0 is sender
  clientApps.Start (ns3::Seconds (0.5)); // Start after a small delay to ensure sink is ready
  clientApps.Stop (ns3::Seconds (10.5)); // Stop after 10 seconds of sending

  // Configure overall simulation time
  ns3::Simulator::Stop (ns3::Seconds (11.0));
  ns3::Simulator::Run ();
  ns3::Simulator::Destroy ();

  return 0;
}