#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int
main (int argc, char *argv[])
{
  // Enable logging for applications if needed (optional)
  // LogComponentEnable ("BulkSendApplication", LOG_LEVEL_INFO);
  // LogComponentEnable ("PacketSink", LOG_LEVEL_INFO);

  // Create the two nodes
  NodeContainer nodes;
  nodes.Create (2);

  // Set up point-to-point helper with given attributes
  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));

  // Install devices and channels
  NetDeviceContainer devices = pointToPoint.Install (nodes);

  // Install the internet stack on the nodes
  InternetStackHelper stack;
  stack.Install (nodes);

  // Assign IP addresses
  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  // Set up TCP server (PacketSink) on node 1
  uint16_t sinkPort = 8080;
  Address sinkAddress (InetSocketAddress (interfaces.GetAddress (1), sinkPort));
  PacketSinkHelper packetSinkHelper ("ns3::TcpSocketFactory", sinkAddress);
  ApplicationContainer sinkApps = packetSinkHelper.Install (nodes.Get (1));
  sinkApps.Start (Seconds (1.0));
  sinkApps.Stop (Seconds (5.0));

  // Set up TCP client (BulkSendApplication) on node 0
  BulkSendHelper bulkSendHelper ("ns3::TcpSocketFactory", sinkAddress);
  bulkSendHelper.SetAttribute ("MaxBytes", UintegerValue (0)); // unlimited
  ApplicationContainer sourceApps = bulkSendHelper.Install (nodes.Get (0));
  sourceApps.Start (Seconds (2.0));
  sourceApps.Stop (Seconds (5.0));

  // Enable pcap tracing, if you want to inspect generated packets (optional)
  // pointToPoint.EnablePcapAll ("tcp-p2p");

  // Run the simulation
  Simulator::Stop (Seconds (5.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}