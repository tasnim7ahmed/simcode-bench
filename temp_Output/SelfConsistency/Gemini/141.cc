#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/log.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("RingNetwork");

int main (int argc, char *argv[])
{
  LogComponent::SetAttribute ("Packet::DropCount", "Mask", StringValue ("~0"));

  // Enable logging
  LogComponent::Enable ("UdpClient", LOG_LEVEL_INFO);
  LogComponent::Enable ("UdpServer", LOG_LEVEL_INFO);
  LogComponent::Enable ("RingNetwork", LOG_LEVEL_INFO);

  // Create nodes
  NodeContainer nodes;
  nodes.Create (3);

  // Create point-to-point links
  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));

  // Create net devices
  NetDeviceContainer devices1 = pointToPoint.Install (nodes.Get (0), nodes.Get (1));
  NetDeviceContainer devices2 = pointToPoint.Install (nodes.Get (1), nodes.Get (2));
  NetDeviceContainer devices3 = pointToPoint.Install (nodes.Get (2), nodes.Get (0));

  // Install Internet stack
  InternetStackHelper stack;
  stack.Install (nodes);

  // Assign IP addresses
  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces1 = address.Assign (devices1);

  address.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces2 = address.Assign (devices2);

  address.SetBase ("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces3 = address.Assign (devices3);

  // Create UDP server on node 2
  UdpServerHelper server (9);
  ApplicationContainer serverApps = server.Install (nodes.Get (2));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  // Create UDP client on node 0
  UdpClientHelper client (interfaces3.GetAddress (1), 9);
  client.SetAttribute ("MaxPackets", UintegerValue (100));
  client.SetAttribute ("Interval", TimeValue (Seconds (0.01)));
  client.SetAttribute ("PacketSize", UintegerValue (1024));

  ApplicationContainer clientApps = client.Install (nodes.Get (0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  Simulator::Stop (Seconds (11.0));

  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}