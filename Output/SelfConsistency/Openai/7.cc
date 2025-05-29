#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("RingTopologyUdpEcho");

int main (int argc, char *argv[])
{
  Time::SetResolution (Time::NS);
  LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
  LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);

  // Create four nodes
  NodeContainer nodes;
  nodes.Create (4);

  // Create point-to-point links in a ring: 0-1, 1-2, 2-3, 3-0
  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer devices[4];
  devices[0] = pointToPoint.Install (nodes.Get(0), nodes.Get(1)); // 0-1
  devices[1] = pointToPoint.Install (nodes.Get(1), nodes.Get(2)); // 1-2
  devices[2] = pointToPoint.Install (nodes.Get(2), nodes.Get(3)); // 2-3
  devices[3] = pointToPoint.Install (nodes.Get(3), nodes.Get(0)); // 3-0

  InternetStackHelper stack;
  stack.Install (nodes);

  // Assign IP addresses: each link gets a /30 subnetwork from 192.9.39.0/24
  Ipv4AddressHelper address;
  std::vector<Ipv4InterfaceContainer> interfaces (4);

  address.SetBase ("192.9.39.0", "255.255.255.252"); // .0 - .3
  interfaces[0] = address.Assign (devices[0]);
  address.SetBase ("192.9.39.4", "255.255.255.252"); // .4 - .7
  interfaces[1] = address.Assign (devices[1]);
  address.SetBase ("192.9.39.8", "255.255.255.252"); // .8 - .11
  interfaces[2] = address.Assign (devices[2]);
  address.SetBase ("192.9.39.12", "255.255.255.252"); // .12 - .15
  interfaces[3] = address.Assign (devices[3]);

  // Assign static routing for full connectivity
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // -- App parameters --
  uint16_t port = 9;
  uint32_t maxPackets = 4;
  Time interval = Seconds (1.0);
  uint32_t packetSize = 1024;

  // Each node acts as both client and server at least once.
  // Only one pair active at a time.
  //
  // Schedule:
  // 0->1 (0 as client, 1 as server): 0s-4s
  // 1->2 (1 as client, 2 as server): 5s-9s
  // 2->3 (2 as client, 3 as server): 10s-14s
  // 3->0 (3 as client, 0 as server): 15s-19s

  // 0->1
  UdpEchoServerHelper echoServer1 (port);
  ApplicationContainer serverApps1 = echoServer1.Install (nodes.Get (1));
  serverApps1.Start (Seconds (0.0));
  serverApps1.Stop (Seconds (4.5));

  UdpEchoClientHelper echoClient1 (interfaces[0].GetAddress (1), port);
  echoClient1.SetAttribute ("MaxPackets", UintegerValue (maxPackets));
  echoClient1.SetAttribute ("Interval", TimeValue (interval));
  echoClient1.SetAttribute ("PacketSize", UintegerValue (packetSize));
  ApplicationContainer clientApps1 = echoClient1.Install (nodes.Get (0));
  clientApps1.Start (Seconds (0.5));
  clientApps1.Stop (Seconds (4.5));

  // 1->2
  UdpEchoServerHelper echoServer2 (port);
  ApplicationContainer serverApps2 = echoServer2.Install (nodes.Get (2));
  serverApps2.Start (Seconds (5.0));
  serverApps2.Stop (Seconds (9.5));

  UdpEchoClientHelper echoClient2 (interfaces[1].GetAddress (1), port);
  echoClient2.SetAttribute ("MaxPackets", UintegerValue (maxPackets));
  echoClient2.SetAttribute ("Interval", TimeValue (interval));
  echoClient2.SetAttribute ("PacketSize", UintegerValue (packetSize));
  ApplicationContainer clientApps2 = echoClient2.Install (nodes.Get (1));
  clientApps2.Start (Seconds (5.5));
  clientApps2.Stop (Seconds (9.5));

  // 2->3
  UdpEchoServerHelper echoServer3 (port);
  ApplicationContainer serverApps3 = echoServer3.Install (nodes.Get (3));
  serverApps3.Start (Seconds (10.0));
  serverApps3.Stop (Seconds (14.5));

  UdpEchoClientHelper echoClient3 (interfaces[2].GetAddress (1), port);
  echoClient3.SetAttribute ("MaxPackets", UintegerValue (maxPackets));
  echoClient3.SetAttribute ("Interval", TimeValue (interval));
  echoClient3.SetAttribute ("PacketSize", UintegerValue (packetSize));
  ApplicationContainer clientApps3 = echoClient3.Install (nodes.Get (2));
  clientApps3.Start (Seconds (10.5));
  clientApps3.Stop (Seconds (14.5));

  // 3->0
  UdpEchoServerHelper echoServer0 (port);
  ApplicationContainer serverApps0 = echoServer0.Install (nodes.Get (0));
  serverApps0.Start (Seconds (15.0));
  serverApps0.Stop (Seconds (19.5));

  UdpEchoClientHelper echoClient0 (interfaces[3].GetAddress (1), port);
  echoClient0.SetAttribute ("MaxPackets", UintegerValue (maxPackets));
  echoClient0.SetAttribute ("Interval", TimeValue (interval));
  echoClient0.SetAttribute ("PacketSize", UintegerValue (packetSize));
  ApplicationContainer clientApps0 = echoClient0.Install (nodes.Get (3));
  clientApps0.Start (Seconds (15.5));
  clientApps0.Stop (Seconds (19.5));

  Simulator::Stop (Seconds (20.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}