#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/ospf-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("OSPFSimulation");

int main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);
  LogComponent::SetLevel (Ipv4GlobalRouting::TypeId(), LOG_LEVEL_ALL);
  LogComponent::SetLevel (OspfProtocol::TypeId(), LOG_LEVEL_ALL);

  NodeContainer nodes;
  nodes.Create (4);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer devices12 = p2p.Install (nodes.Get (0), nodes.Get (1));
  NetDeviceContainer devices13 = p2p.Install (nodes.Get (0), nodes.Get (2));
  NetDeviceContainer devices23 = p2p.Install (nodes.Get (1), nodes.Get (2));
  NetDeviceContainer devices24 = p2p.Install (nodes.Get (1), nodes.Get (3));
  NetDeviceContainer devices34 = p2p.Install (nodes.Get (2), nodes.Get (3));

  InternetStackHelper internet;
  OspfHelper ospf;
  internet.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces12 = address.Assign (devices12);
  address.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces13 = address.Assign (devices13);
  address.SetBase ("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces23 = address.Assign (devices23);
  address.SetBase ("10.1.4.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces24 = address.Assign (devices24);
  address.SetBase ("10.1.5.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces34 = address.Assign (devices34);

  ospf.Install (nodes.Get (0));
  ospf.Install (nodes.Get (1));
  ospf.Install (nodes.Get (2));
  ospf.Install (nodes.Get (3));

  UdpEchoServerHelper echoServer (9);
  ApplicationContainer serverApps = echoServer.Install (nodes.Get (3));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  UdpEchoClientHelper echoClient (interfaces34.GetAddress (1), 9);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (10));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));
  ApplicationContainer clientApps = echoClient.Install (nodes.Get (0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  p2p.EnablePcapAll("ospf-simulation");

  AnimationInterface anim ("ospf-animation.xml");
  anim.SetConstantPosition (nodes.Get (0), 10, 10);
  anim.SetConstantPosition (nodes.Get (1), 40, 10);
  anim.SetConstantPosition (nodes.Get (2), 10, 40);
  anim.SetConstantPosition (nodes.Get (3), 40, 40);

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}