#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("StarTopology");

int main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);
  LogComponent::SetFirstLevel (LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create (5);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer devices01 = pointToPoint.Install (nodes.Get (0), nodes.Get (1));
  NetDeviceContainer devices02 = pointToPoint.Install (nodes.Get (0), nodes.Get (2));
  NetDeviceContainer devices03 = pointToPoint.Install (nodes.Get (0), nodes.Get (3));
  NetDeviceContainer devices04 = pointToPoint.Install (nodes.Get (0), nodes.Get (4));

  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("192.9.39.0", "255.255.255.0");

  Ipv4InterfaceContainer interfaces01 = address.Assign (devices01);
  Ipv4InterfaceContainer interfaces02 = address.Assign (devices02);
  Ipv4InterfaceContainer interfaces03 = address.Assign (devices03);
  Ipv4InterfaceContainer interfaces04 = address.Assign (devices04);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  uint16_t echoPort = 9;

  UdpEchoServerHelper echoServer (echoPort);
  ApplicationContainer serverApps = echoServer.Install (nodes.Get (1));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  UdpEchoClientHelper echoClient (interfaces01.GetAddress (1), echoPort);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (4));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));
  ApplicationContainer clientApps = echoClient.Install (nodes.Get (0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  serverApps = echoServer.Install (nodes.Get (2));
  serverApps.Start (Seconds (11.0));
  serverApps.Stop (Seconds (20.0));

  echoClient.SetAttribute ("MaxPackets", UintegerValue (4));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));
  echoClient.SetRemoteAddress(interfaces02.GetAddress(1));
  clientApps = echoClient.Install (nodes.Get (0));
  clientApps.Start (Seconds (12.0));
  clientApps.Stop (Seconds (20.0));

  serverApps = echoServer.Install (nodes.Get (3));
  serverApps.Start (Seconds (21.0));
  serverApps.Stop (Seconds (30.0));

  echoClient.SetAttribute ("MaxPackets", UintegerValue (4));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));
  echoClient.SetRemoteAddress(interfaces03.GetAddress(1));
  clientApps = echoClient.Install (nodes.Get (0));
  clientApps.Start (Seconds (22.0));
  clientApps.Stop (Seconds (30.0));

  serverApps = echoServer.Install (nodes.Get (4));
  serverApps.Start (Seconds (31.0));
  serverApps.Stop (Seconds (40.0));

  echoClient.SetAttribute ("MaxPackets", UintegerValue (4));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));
  echoClient.SetRemoteAddress(interfaces04.GetAddress(1));
  clientApps = echoClient.Install (nodes.Get (0));
  clientApps.Start (Seconds (32.0));
  clientApps.Stop (Seconds (40.0));

  AnimationInterface anim ("star-topology.xml");
  anim.SetConstantPosition (nodes.Get (0), 10.0, 10.0);
  anim.SetConstantPosition (nodes.Get (1), 20.0, 20.0);
  anim.SetConstantPosition (nodes.Get (2), 30.0, 20.0);
  anim.SetConstantPosition (nodes.Get (3), 20.0, 30.0);
  anim.SetConstantPosition (nodes.Get (4), 30.0, 30.0);

  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}