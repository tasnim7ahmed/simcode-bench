#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/animation-interface.h"

using namespace ns3;

int
main (int argc, char *argv[])
{
  // LogComponentEnable ("BulkSendApplication", LOG_LEVEL_INFO);
  // Simulation time and visualization parameters
  double simTime = 10.0; // seconds

  // Set TCP variant to NewReno
  Config::SetDefault ("ns3::TcpL4Protocol::SocketType",
                      TypeIdValue (TcpNewReno::GetTypeId ()));

  // Create nodes
  NodeContainer nodes;
  nodes.Create (2);

  // Create point-to-point channel
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer devices = p2p.Install (nodes);

  // Install Internet stack
  InternetStackHelper stack;
  stack.Install (nodes);

  // Assign IP addresses
  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  // Install BulkSendApplication on node 0, connecting to node 1
  uint16_t port = 50000;

  BulkSendHelper bulkSender ("ns3::TcpSocketFactory",
      InetSocketAddress (interfaces.GetAddress (1), port));
  bulkSender.SetAttribute ("MaxBytes", UintegerValue (0)); // unlimited

  ApplicationContainer senderApps = bulkSender.Install (nodes.Get (0));
  senderApps.Start (Seconds (1.0));
  senderApps.Stop (Seconds (simTime));

  // Install PacketSink on node 1
  PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory",
      InetSocketAddress (Ipv4Address::GetAny (), port));
  ApplicationContainer sinkApps = sinkHelper.Install (nodes.Get (1));
  sinkApps.Start (Seconds (0.0));
  sinkApps.Stop (Seconds (simTime));

  // Enable NetAnim tracing and assign positions for visualization
  AnimationInterface anim ("wired-tcp-bulk-anim.xml");
  anim.SetConstantPosition (nodes.Get (0), 10, 30);
  anim.SetConstantPosition (nodes.Get (1), 60, 30);

  // Enable pcap tracing for packet-level visualization
  p2p.EnablePcapAll ("wired-tcp-bulk");

  // Run simulation
  Simulator::Stop (Seconds (simTime));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}