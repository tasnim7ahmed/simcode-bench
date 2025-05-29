#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("RingNetwork");

int main (int argc, char *argv[])
{
  LogComponent::SetAttribute ("UdpEchoClientApplication", "PacketSize", UintegerValue (1024));
  CommandLine cmd;
  cmd.Parse (argc, argv);

  NodeContainer nodes;
  nodes.Create (4);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer devices;
  for (uint32_t i = 0; i < 4; ++i)
  {
    NetDeviceContainer device;
    device = pointToPoint.Install (nodes.Get (i), nodes.Get ((i + 1) % 4));
    devices.Add (device.Get (0));
    devices.Add (device.Get (1));
  }

  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("192.9.39.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  UdpEchoServerHelper echoServer (9);
  ApplicationContainer serverApps = echoServer.Install (nodes.Get (0));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  UdpEchoClientHelper echoClient (interfaces.GetAddress (1), 9);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (4));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));
  ApplicationContainer clientApps = echoClient.Install (nodes.Get (1));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  UdpEchoServerHelper echoServer1 (9);
  ApplicationContainer serverApps1 = echoServer1.Install (nodes.Get (1));
  serverApps1.Start (Seconds (11.0));
  serverApps1.Stop (Seconds (20.0));

  UdpEchoClientHelper echoClient1 (interfaces.GetAddress (3), 9);
  echoClient1.SetAttribute ("MaxPackets", UintegerValue (4));
  echoClient1.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient1.SetAttribute ("PacketSize", UintegerValue (1024));
  ApplicationContainer clientApps1 = echoClient1.Install (nodes.Get (3));
  clientApps1.Start (Seconds (12.0));
  clientApps1.Stop (Seconds (20.0));

  UdpEchoServerHelper echoServer2 (9);
  ApplicationContainer serverApps2 = echoServer2.Install (nodes.Get (2));
  serverApps2.Start (Seconds (21.0));
  serverApps2.Stop (Seconds (30.0));

  UdpEchoClientHelper echoClient2 (interfaces.GetAddress (5), 9);
  echoClient2.SetAttribute ("MaxPackets", UintegerValue (4));
  echoClient2.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient2.SetAttribute ("PacketSize", UintegerValue (1024));
  ApplicationContainer clientApps2 = echoClient2.Install (nodes.Get (0));
  clientApps2.Start (Seconds (22.0));
  clientApps2.Stop (Seconds (30.0));

  UdpEchoServerHelper echoServer3 (9);
  ApplicationContainer serverApps3 = echoServer3.Install (nodes.Get (3));
  serverApps3.Start (Seconds (31.0));
  serverApps3.Stop (Seconds (40.0));

  UdpEchoClientHelper echoClient3 (interfaces.GetAddress (7), 9);
  echoClient3.SetAttribute ("MaxPackets", UintegerValue (4));
  echoClient3.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient3.SetAttribute ("PacketSize", UintegerValue (1024));
  ApplicationContainer clientApps3 = echoClient3.Install (nodes.Get (2));
  clientApps3.Start (Seconds (32.0));
  clientApps3.Stop (Seconds (40.0));

  AnimationInterface anim ("ring-network.xml");
  anim.SetConstantPosition (nodes.Get (0), 10.0, 10.0);
  anim.SetConstantPosition (nodes.Get (1), 30.0, 10.0);
  anim.SetConstantPosition (nodes.Get (2), 30.0, 30.0);
  anim.SetConstantPosition (nodes.Get (3), 10.0, 30.0);

  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}