#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("ThreeNodeStaticRouting");

int main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);
  LogComponent::SetAttribute ("UdpClient", "Interval", StringValue ("0.01"));

  NodeContainer nodes;
  nodes.Create (3);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer devicesAB = pointToPoint.Install (nodes.Get (0), nodes.Get (1));
  NetDeviceContainer devicesBC = pointToPoint.Install (nodes.Get (1), nodes.Get (2));

  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfacesAB = address.Assign (devicesAB);

  address.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer interfacesBC = address.Assign (devicesBC);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  Ipv4StaticRoutingHelper staticRouting;
  Ptr<Ipv4StaticRouting> routerA = staticRouting.GetStaticRouting (nodes.Get (0)->GetObject<Ipv4> ());
  routerA->AddHostRouteToNetwork (Ipv4Address ("10.1.2.2"), Ipv4Address ("10.1.1.2"), 1);

  Ptr<Ipv4StaticRouting> routerB = staticRouting.GetStaticRouting (nodes.Get (1)->GetObject<Ipv4> ());
  routerB->AddHostRouteToNetwork (Ipv4Address ("10.1.2.2"), Ipv4Address ("10.1.2.2"), 1);

  OnOffHelper onoff ("ns3::UdpSocketFactory", Address (InetSocketAddress (interfacesBC.GetAddress (1), 9)));
  onoff.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
  onoff.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
  onoff.SetAttribute ("PacketSize", UintegerValue (1024));
  ApplicationContainer onoffApp = onoff.Install (nodes.Get (0));
  onoffApp.Start (Seconds (1.0));
  onoffApp.Stop (Seconds (10.0));

  PacketSinkHelper sink ("ns3::UdpSocketFactory", InetSocketAddress (interfacesBC.GetAddress (1), 9));
  ApplicationContainer sinkApp = sink.Install (nodes.Get (2));
  sinkApp.Start (Seconds (1.0));
  sinkApp.Stop (Seconds (10.0));

  pointToPoint.EnablePcapAll ("three-node-static-routing");

  AnimationInterface anim ("three-node-static-routing.xml");
  anim.SetConstantPosition (nodes.Get (0), 0.0, 0.0);
  anim.SetConstantPosition (nodes.Get (1), 50.0, 0.0);
  anim.SetConstantPosition (nodes.Get (2), 100.0, 0.0);

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}