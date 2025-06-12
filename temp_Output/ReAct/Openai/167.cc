#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

void
CalculateThroughput (Ptr<PacketSink> sink, Time lastTime, uint64_t lastTotalRx)
{
  Time now = Simulator::Now ();
  uint64_t curTotalRx = sink->GetTotalRx ();
  double throughput = ((curTotalRx - lastTotalRx) * 8.0) / (now.GetSeconds () - lastTime.GetSeconds ()) / 1e6;
  std::cout << "Time: " << now.GetSeconds () << "s, Throughput: " << throughput << " Mbps" << std::endl;
  Simulator::Schedule (Seconds (1.0), &CalculateThroughput, sink, now, curTotalRx);
}

int
main (int argc, char *argv[])
{
  // Enable logging if needed
  // LogComponentEnable ("PacketSink", LOG_LEVEL_INFO);
  // LogComponentEnable ("OnOffApplication", LOG_LEVEL_INFO);

  // Create 2 nodes
  NodeContainer nodes;
  nodes.Create (2);

  // Configure the point-to-point link
  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));

  // Install devices
  NetDeviceContainer devices;
  devices = pointToPoint.Install (nodes);

  // Enable pcap and ascii tracing
  pointToPoint.EnablePcapAll ("tcp-p2p");
  AsciiTraceHelper ascii;
  pointToPoint.EnableAsciiAll (ascii.CreateFileStream ("tcp-p2p.tr"));

  // Install internet stack
  InternetStackHelper stack;
  stack.Install (nodes);

  // Assign IP addresses
  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  // Create and configure the TCP server (PacketSink) on Node A (node 0)
  uint16_t port = 50000;
  Address sinkAddress (InetSocketAddress (interfaces.GetAddress (0), port));
  PacketSinkHelper packetSinkHelper ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny(), port));
  ApplicationContainer sinkApp = packetSinkHelper.Install (nodes.Get (0));
  sinkApp.Start (Seconds (0.0));
  sinkApp.Stop (Seconds (11.0));

  // Create and configure TCP client (OnOffApplication) on Node B (node 1)
  OnOffHelper clientHelper ("ns3::TcpSocketFactory", sinkAddress);
  clientHelper.SetAttribute ("DataRate", StringValue ("5Mbps"));
  clientHelper.SetAttribute ("PacketSize", UintegerValue (1024));
  clientHelper.SetAttribute ("MaxBytes", UintegerValue (0)); // unlimited
  ApplicationContainer clientApp = clientHelper.Install (nodes.Get (1));
  clientApp.Start (Seconds (1.0));
  clientApp.Stop (Seconds (10.0));

  // Throughput measurement scheduled every second
  Simulator::Schedule (Seconds (1.1), &CalculateThroughput, DynamicCast<PacketSink> (sinkApp.Get (0)), Seconds (1.0), 0);

  Simulator::Stop (Seconds (11.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}