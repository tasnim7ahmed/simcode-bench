#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/packet-sink.h"
#include "ns3/packet-sink-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("RingNetwork");

int main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);
  LogComponent::SetAttribute ("UdpClient", "PacketSize", UintegerValue (1024));

  NodeContainer nodes;
  nodes.Create (4);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("1ms"));

  NetDeviceContainer devices[4];

  devices[0] = pointToPoint.Install (nodes.Get (0), nodes.Get (1));
  devices[1] = pointToPoint.Install (nodes.Get (1), nodes.Get (2));
  devices[2] = pointToPoint.Install (nodes.Get (2), nodes.Get (3));
  devices[3] = pointToPoint.Install (nodes.Get (3), nodes.Get (0));

  InternetStackHelper stack;
  stack.Install (nodes);

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

  UdpEchoServerHelper echoServer (9);
  ApplicationContainer serverApps = echoServer.Install (nodes.Get (1));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  UdpEchoClientHelper echoClient (interfaces[0].GetAddress (1), 9);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (100));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (0.01)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));
  ApplicationContainer clientApps = echoClient.Install (nodes.Get (0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  UdpEchoServerHelper echoServer2 (9);
  ApplicationContainer serverApps2 = echoServer2.Install (nodes.Get (2));
  serverApps2.Start (Seconds (1.0));
  serverApps2.Stop (Seconds (10.0));

  UdpEchoClientHelper echoClient2 (interfaces[1].GetAddress (1), 9);
  echoClient2.SetAttribute ("MaxPackets", UintegerValue (100));
  echoClient2.SetAttribute ("Interval", TimeValue (Seconds (0.01)));
  echoClient2.SetAttribute ("PacketSize", UintegerValue (1024));
  ApplicationContainer clientApps2 = echoClient2.Install (nodes.Get (1));
  clientApps2.Start (Seconds (2.5));
  clientApps2.Stop (Seconds (10.0));

  UdpEchoServerHelper echoServer3 (9);
  ApplicationContainer serverApps3 = echoServer3.Install (nodes.Get (3));
  serverApps3.Start (Seconds (1.0));
  serverApps3.Stop (Seconds (10.0));

  UdpEchoClientHelper echoClient3 (interfaces[2].GetAddress (1), 9);
  echoClient3.SetAttribute ("MaxPackets", UintegerValue (100));
  echoClient3.SetAttribute ("Interval", TimeValue (Seconds (0.01)));
  echoClient3.SetAttribute ("PacketSize", UintegerValue (1024));
  ApplicationContainer clientApps3 = echoClient3.Install (nodes.Get (2));
  clientApps3.Start (Seconds (3.0));
  clientApps3.Stop (Seconds (10.0));

  UdpEchoServerHelper echoServer4 (9);
  ApplicationContainer serverApps4 = echoServer4.Install (nodes.Get (0));
  serverApps4.Start (Seconds (1.0));
  serverApps4.Stop (Seconds (10.0));

  UdpEchoClientHelper echoClient4 (interfaces[3].GetAddress (1), 9);
  echoClient4.SetAttribute ("MaxPackets", UintegerValue (100));
  echoClient4.SetAttribute ("Interval", TimeValue (Seconds (0.01)));
  echoClient4.SetAttribute ("PacketSize", UintegerValue (1024));
  ApplicationContainer clientApps4 = echoClient4.Install (nodes.Get (3));
  clientApps4.Start (Seconds (3.5));
  clientApps4.Stop (Seconds (10.0));

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}