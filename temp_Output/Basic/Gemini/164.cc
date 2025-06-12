#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/ospf-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("OspfNetAnim");

int main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);
  LogComponent::SetDefaultLogLevel (LogLevel::LOG_LEVEL_INFO);
  LogComponent::SetPrintMaskForAll (true);

  NodeContainer nodes;
  nodes.Create (5);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer devices1 = pointToPoint.Install (nodes.Get (0), nodes.Get (1));
  NetDeviceContainer devices2 = pointToPoint.Install (nodes.Get (1), nodes.Get (2));
  NetDeviceContainer devices3 = pointToPoint.Install (nodes.Get (2), nodes.Get (3));
  NetDeviceContainer devices4 = pointToPoint.Install (nodes.Get (3), nodes.Get (4));
  NetDeviceContainer devices5 = pointToPoint.Install (nodes.Get (0), nodes.Get (4));
  NetDeviceContainer devices6 = pointToPoint.Install (nodes.Get (1), nodes.Get (4));

  InternetStackHelper internet;
  OspfHelper ospfHelper;
  internet.AddProtocol (&ospfHelper, 0);
  internet.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces1 = address.Assign (devices1);
  address.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces2 = address.Assign (devices2);
  address.SetBase ("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces3 = address.Assign (devices3);
  address.SetBase ("10.1.4.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces4 = address.Assign (devices4);
  address.SetBase ("10.1.5.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces5 = address.Assign (devices5);
  address.SetBase ("10.1.6.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces6 = address.Assign (devices6);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  uint16_t port = 9;
  UdpEchoServerHelper echoServer (port);

  ApplicationContainer serverApps = echoServer.Install (nodes.Get (4));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  UdpEchoClientHelper echoClient (interfaces4.GetAddress (1), port);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (10));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));

  ApplicationContainer clientApps = echoClient.Install (nodes.Get (0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  AnimationInterface anim ("ospf-netanim.xml");
  anim.SetConstantPosition (nodes.Get (0), 10, 10);
  anim.SetConstantPosition (nodes.Get (1), 50, 10);
  anim.SetConstantPosition (nodes.Get (2), 90, 10);
  anim.SetConstantPosition (nodes.Get (3), 90, 50);
  anim.SetConstantPosition (nodes.Get (4), 50, 50);

  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}