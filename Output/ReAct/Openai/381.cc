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

  // Create two nodes
  NodeContainer nodes;
  nodes.Create (2);

  // Create point-to-point channel
  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("5ms"));

  // Install devices
  NetDeviceContainer devices;
  devices = pointToPoint.Install (nodes);

  // Install TCP/IP stack
  InternetStackHelper stack;
  stack.Install (nodes);

  // Assign IP addresses
  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  // Install TCP server (PacketSink) on Node 1
  uint16_t port = 50000;
  Address sinkAddress (InetSocketAddress (interfaces.GetAddress (1), port));
  PacketSinkHelper packetSinkHelper ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), port));
  ApplicationContainer sinkApps = packetSinkHelper.Install (nodes.Get (1));
  sinkApps.Start (Seconds (0.0));
  sinkApps.Stop (Seconds (10.0));

  // Install TCP client (OnOffApplication) on Node 0
  OnOffHelper clientHelper ("ns3::TcpSocketFactory", sinkAddress);
  clientHelper.SetAttribute ("DataRate", DataRateValue (DataRate ("1Mbps")));
  clientHelper.SetAttribute ("PacketSize", UintegerValue (1024));
  clientHelper.SetAttribute ("StartTime", TimeValue (Seconds (1.0)));
  clientHelper.SetAttribute ("StopTime", TimeValue (Seconds (10.0)));

  ApplicationContainer clientApps = clientHelper.Install (nodes.Get (0));

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}