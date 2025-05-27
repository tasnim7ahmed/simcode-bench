#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/netanim-module.h"
#include "ns3/csma-module.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-list-routing.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("MulticastExample");

int main (int argc, char *argv[])
{
  bool verbose = false;
  bool tracing = false;

  CommandLine cmd;
  cmd.AddValue ("verbose", "Tell application to log if true", verbose);
  cmd.AddValue ("tracing", "Enable pcap tracing", tracing);

  cmd.Parse (argc, argv);

  if (verbose)
    {
      LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
      LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);
    }

  Time::SetResolution (Time::NS);

  NodeContainer nodes;
  nodes.Create (5);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer devicesA_B, devicesA_C, devicesA_D, devicesA_E;
  devicesA_B = p2p.Install (nodes.Get (0), nodes.Get (1));
  devicesA_C = p2p.Install (nodes.Get (0), nodes.Get (2));
  devicesA_D = p2p.Install (nodes.Get (0), nodes.Get (3));
  devicesA_E = p2p.Install (nodes.Get (0), nodes.Get (4));

  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfacesA_B = address.Assign (devicesA_B);

  address.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer interfacesA_C = address.Assign (devicesA_C);

  address.SetBase ("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer interfacesA_D = address.Assign (devicesA_D);

  address.SetBase ("10.1.4.0", "255.255.255.0");
  Ipv4InterfaceContainer interfacesA_E = address.Assign (devicesA_E);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // Set up multicast routing
  Ipv4StaticRoutingHelper staticRouting;

  Ptr<Ipv4StaticRouting> routingB = staticRouting.GetStaticRouting (nodes.Get (1)->GetObject<Ipv4> ());
  routingB->AddMulticastRoute (Ipv4Address ("225.1.2.3"), interfacesA_B.GetAddress (1));

  Ptr<Ipv4StaticRouting> routingC = staticRouting.GetStaticRouting (nodes.Get (2)->GetObject<Ipv4> ());
  routingC->AddMulticastRoute (Ipv4Address ("225.1.2.3"), interfacesA_C.GetAddress (1));

  Ptr<Ipv4StaticRouting> routingD = staticRouting.GetStaticRouting (nodes.Get (3)->GetObject<Ipv4> ());
  routingD->AddMulticastRoute (Ipv4Address ("225.1.2.3"), interfacesA_D.GetAddress (1));

  Ptr<Ipv4StaticRouting> routingE = staticRouting.GetStaticRouting (nodes.Get (4)->GetObject<Ipv4> ());
  routingE->AddMulticastRoute (Ipv4Address ("225.1.2.3"), interfacesA_E.GetAddress (1));

  // Blacklist link between A and D
  p2p.SetDeviceAttribute ("BlackList", StringValue (interfacesA_D.GetAddress (1).Get().ConvertToString()));
  p2p.SetChannelAttribute ("Delay", StringValue ("1000ms"));
  NetDeviceContainer temp;
  temp = p2p.Install(nodes.Get(0),nodes.Get(3));

  uint16_t port = 9;

  // Create packet sinks on nodes B, C, D, and E
  Address multicastLocalAddress (InetSocketAddress (Ipv4Address::GetAny (), port));
  PacketSinkHelper sinkHelper ("ns3::UdpSocketFactory", multicastLocalAddress);

  ApplicationContainer sinkAppsB = sinkHelper.Install (nodes.Get (1));
  sinkAppsB.Start (Seconds (1.0));
  sinkAppsB.Stop (Seconds (10.0));

  ApplicationContainer sinkAppsC = sinkHelper.Install (nodes.Get (2));
  sinkAppsC.Start (Seconds (1.0));
  sinkAppsC.Stop (Seconds (10.0));

  ApplicationContainer sinkAppsD = sinkHelper.Install (nodes.Get (3));
  sinkAppsD.Start (Seconds (1.0));
  sinkAppsD.Stop (Seconds (10.0));

  ApplicationContainer sinkAppsE = sinkHelper.Install (nodes.Get (4));
  sinkAppsE.Start (Seconds (1.0));
  sinkAppsE.Stop (Seconds (10.0));

  // Create OnOff application on node A for multicast
  OnOffHelper onoff ("ns3::UdpSocketFactory", Address (InetSocketAddress (Ipv4Address ("225.1.2.3"), port)));
  onoff.SetConstantRate (DataRate ("100kbps"));
  onoff.SetAttribute ("PacketSize", UintegerValue (1472));

  ApplicationContainer clientApps = onoff.Install (nodes.Get (0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (9.0));

  if (tracing)
    {
       p2p.EnablePcapAll ("multicast");
    }

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}