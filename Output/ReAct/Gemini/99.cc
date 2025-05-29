#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("ThreeRouterNetwork");

int main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);
  LogComponent::SetLogLevel (LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create (3);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer devices01 = pointToPoint.Install (nodes.Get (0), nodes.Get (1));
  NetDeviceContainer devices12 = pointToPoint.Install (nodes.Get (1), nodes.Get (2));

  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces01 = address.Assign (devices01);

  address.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces12 = address.Assign (devices12);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  Ptr<Ipv4StaticRouting> staticRoutingA = Ipv4RoutingHelper::GetStaticRouting (nodes.Get (0)->GetObject<Ipv4> ());
  staticRoutingA->AddHostRouteToNetwork (Ipv4Address ("10.1.2.2"), Ipv4Address ("255.255.255.255"), 1);

  Ptr<Ipv4StaticRouting> staticRoutingC = Ipv4RoutingHelper::GetStaticRouting (nodes.Get (2)->GetObject<Ipv4> ());
  staticRoutingC->AddHostRouteToNetwork (Ipv4Address ("10.1.1.1"), Ipv4Address ("255.255.255.255"), 1);

  // Inject routes at node B
  Ptr<Ipv4StaticRouting> staticRoutingB = Ipv4RoutingHelper::GetStaticRouting (nodes.Get (1)->GetObject<Ipv4> ());
  staticRoutingB->AddHostRouteToNetwork (Ipv4Address ("10.1.1.1"), Ipv4Address ("255.255.255.255"), interfaces01.GetInterface (1)->GetIfIndex());
  staticRoutingB->AddHostRouteToNetwork (Ipv4Address ("10.1.2.2"), Ipv4Address ("255.255.255.255"), interfaces12.GetInterface (0)->GetIfIndex());

  uint16_t port = 9;
  OnOffHelper onOff ("ns3::UdpSocketFactory",
                     Address (InetSocketAddress (interfaces12.GetAddress (1), port)));
  onOff.SetConstantRate (DataRate ("1Mbps"));

  ApplicationContainer app = onOff.Install (nodes.Get (0));
  app.Start (Seconds (1.0));
  app.Stop (Seconds (10.0));

  PacketSinkHelper sink ("ns3::UdpSocketFactory",
                         InetSocketAddress (Ipv4Address::GetAny (), port));
  ApplicationContainer sinkApp = sink.Install (nodes.Get (2));
  sinkApp.Start (Seconds (1.0));
  sinkApp.Stop (Seconds (10.0));

  pointToPoint.EnablePcapAll ("three-router");

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}