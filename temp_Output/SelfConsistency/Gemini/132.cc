#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/log.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("DistanceVectorLoop");

int main (int argc, char *argv[])
{
  LogComponent::SetAttribute ("Packet::Size", UintegerValue (1024));
  LogComponent::SetAttribute ("DistanceVectorRouting", StringValue ("Enable"));
  LogComponent::SetAttribute ("DistanceVectorRouting::RoutingTable::PrintRoutesOnUpdate", BooleanValue (true));
  LogComponent::SetAttribute ("DistanceVectorRouting::RoutingTable::DropPoisonedRoutes", BooleanValue (false));

  CommandLine cmd;
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);

  NodeContainer nodes;
  nodes.Create (2);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer devices;
  devices = pointToPoint.Install (nodes);

  InternetStackHelper internet;
  internet.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  // Create a sink application on node 1 (B) to receive packets
  uint16_t sinkPort = 9;
  Address sinkLocalAddress (InetSocketAddress (Ipv4Address::GetAny (), sinkPort));
  PacketSinkHelper packetSinkHelper ("ns3::UdpSocketFactory", sinkLocalAddress);
  ApplicationContainer sinkApp = packetSinkHelper.Install (nodes.Get (1));
  sinkApp.Start (Seconds (1.0));
  sinkApp.Stop (Seconds (10.0));

  // Create an OnOff application on node 0 (A) to send packets to node 1 (B)
  OnOffHelper onoff ("ns3::UdpSocketFactory", Address (InetSocketAddress (interfaces.GetAddress (1), sinkPort)));
  onoff.SetConstantRate (DataRate ("1Mbps"));
  ApplicationContainer clientApp = onoff.Install (nodes.Get (0));
  clientApp.Start (Seconds (2.0));
  clientApp.Stop (Seconds (4.0));

  // Simulate a link failure by changing the link cost after 5 seconds
  Simulator::Schedule (Seconds (5.0), [&] () {
      NS_LOG_INFO ("Simulating link failure...");
      pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("100Kbps"));
      pointToPoint.SetChannelAttribute ("Delay", StringValue ("100ms"));
      pointToPoint.Install(nodes);
  });

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  Simulator::Stop (Seconds (10.0));

  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}