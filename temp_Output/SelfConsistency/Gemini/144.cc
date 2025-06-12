#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/log.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("RingTopology");

int main (int argc, char *argv[])
{
  LogComponent::SetAttribute ("UdpClient", "Interval", StringValue ("0.01"));
  // Enable logging from the UdpClient application
  LogComponent::Enable ("UdpClient");

  // Enable logging from the UdpServer application
  LogComponent::Enable ("UdpServer");
  
  // Create 4 nodes
  NodeContainer nodes;
  nodes.Create (4);

  // Create point-to-point links between the nodes
  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("1ms"));

  NetDeviceContainer devices[4];
  devices[0] = pointToPoint.Install (nodes.Get (0), nodes.Get (1));
  devices[1] = pointToPoint.Install (nodes.Get (1), nodes.Get (2));
  devices[2] = pointToPoint.Install (nodes.Get (2), nodes.Get (3));
  devices[3] = pointToPoint.Install (nodes.Get (3), nodes.Get (0));
  

  // Install the internet stack on the nodes
  InternetStackHelper internet;
  internet.Install (nodes);

  // Assign IP addresses to the interfaces
  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces[4];
  interfaces[0] = address.Assign (devices[0]);
  address.NewNetwork ();
  interfaces[1] = address.Assign (devices[1]);
  address.NewNetwork ();
  interfaces[2] = address.Assign (devices[2]);
  address.NewNetwork ();
  interfaces[3] = address.Assign (devices[3]);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // Create UDP client and server applications
  uint16_t port = 9;  // Discard port (RFC 863)

  UdpServerHelper server (port);
  ApplicationContainer sinkApps = server.Install (nodes.Get (1));
  sinkApps.Start (Seconds (1.0));
  sinkApps.Stop (Seconds (10.0));

  UdpClientHelper client (interfaces[0].GetAddress (1), port);
  client.SetAttribute ("MaxPackets", UintegerValue (100));
  client.SetAttribute ("Interval", TimeValue (Seconds (0.01)));
  client.SetAttribute ("PacketSize", UintegerValue (1024));
  ApplicationContainer clientApps = client.Install (nodes.Get (0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));
  
  UdpServerHelper server2 (port);
  ApplicationContainer sinkApps2 = server2.Install (nodes.Get (2));
  sinkApps2.Start (Seconds (1.0));
  sinkApps2.Stop (Seconds (10.0));
  
  UdpClientHelper client2 (interfaces[1].GetAddress (1), port);
  client2.SetAttribute ("MaxPackets", UintegerValue (100));
  client2.SetAttribute ("Interval", TimeValue (Seconds (0.01)));
  client2.SetAttribute ("PacketSize", UintegerValue (1024));
  ApplicationContainer clientApps2 = client2.Install (nodes.Get (1));
  clientApps2.Start (Seconds (3.0));
  clientApps2.Stop (Seconds (10.0));

  UdpServerHelper server3 (port);
  ApplicationContainer sinkApps3 = server3.Install (nodes.Get (3));
  sinkApps3.Start (Seconds (1.0));
  sinkApps3.Stop (Seconds (10.0));

  UdpClientHelper client3 (interfaces[2].GetAddress (1), port);
  client3.SetAttribute ("MaxPackets", UintegerValue (100));
  client3.SetAttribute ("Interval", TimeValue (Seconds (0.01)));
  client3.SetAttribute ("PacketSize", UintegerValue (1024));
  ApplicationContainer clientApps3 = client3.Install (nodes.Get (2));
  clientApps3.Start (Seconds (4.0));
  clientApps3.Stop (Seconds (10.0));

  UdpServerHelper server4 (port);
  ApplicationContainer sinkApps4 = server4.Install (nodes.Get (0));
  sinkApps4.Start (Seconds (1.0));
  sinkApps4.Stop (Seconds (10.0));

  UdpClientHelper client4 (interfaces[3].GetAddress (1), port);
  client4.SetAttribute ("MaxPackets", UintegerValue (100));
  client4.SetAttribute ("Interval", TimeValue (Seconds (0.01)));
  client4.SetAttribute ("PacketSize", UintegerValue (1024));
  ApplicationContainer clientApps4 = client4.Install (nodes.Get (3));
  clientApps4.Start (Seconds (5.0));
  clientApps4.Stop (Seconds (10.0));

  // Enable PCAP tracing
  pointToPoint.EnablePcapAll ("ring");

  // Run the simulation
  Simulator::Stop (Seconds (11.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}