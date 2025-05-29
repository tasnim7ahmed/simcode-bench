#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int
main (int argc, char *argv[])
{
  // Enable logging if needed
  // LogComponentEnable ("BulkSendApplication", LOG_LEVEL_INFO);
  // LogComponentEnable ("PacketSink", LOG_LEVEL_INFO);

  // Create two nodes
  NodeContainer nodes;
  nodes.Create (2);

  // Configure point-to-point link attributes
  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));

  // Install devices on the nodes
  NetDeviceContainer devices;
  devices = pointToPoint.Install (nodes);

  // Install Internet stack
  InternetStackHelper stack;
  stack.Install (nodes);

  // Assign IP addresses
  Ipv4AddressHelper address;
  address.SetBase ("192.168.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  // Set up a TCP BulkSendApplication on node 0, dest = node 1 port 8080
  uint16_t sinkPort = 8080;
  Address sinkAddress (InetSocketAddress (interfaces.GetAddress (1), sinkPort));

  BulkSendHelper source ("ns3::TcpSocketFactory", sinkAddress);
  // Unlimited data; MaxBytes is 0 by default
  source.SetAttribute ("MaxBytes", UintegerValue (0));

  ApplicationContainer sourceApps = source.Install (nodes.Get (0));
  sourceApps.Start (Seconds (1.0));
  sourceApps.Stop (Seconds (10.0));

  // Set up TCP PacketSinkApplication on node 1
  PacketSinkHelper sink ("ns3::TcpSocketFactory",
                         InetSocketAddress (Ipv4Address::GetAny (), sinkPort));
  ApplicationContainer sinkApps = sink.Install (nodes.Get (1));
  sinkApps.Start (Seconds (0.0));
  sinkApps.Stop (Seconds (10.1));

  // Enable pcap (optional)
  // pointToPoint.EnablePcapAll ("simple-point-to-point");

  Simulator::Stop (Seconds (10.1));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}