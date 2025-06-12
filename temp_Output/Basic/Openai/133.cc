#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("SplitHorizonDvExample");

int main (int argc, char *argv[])
{
  CommandLine cmd (__FILE__);
  cmd.Parse (argc, argv);

  // Create nodes
  NodeContainer nodes;
  nodes.Create (3);

  // Create point-to-point channels
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

  // Connect Node 0 <-> Node 1
  NetDeviceContainer d0d1 = p2p.Install (nodes.Get (0), nodes.Get (1));
  // Connect Node 1 <-> Node 2
  NetDeviceContainer d1d2 = p2p.Install (nodes.Get (1), nodes.Get (2));

  // Install internet stack with Distance Vector Routing and Split Horizon enabled
  InternetStackHelper stack;
  Ipv4ListRoutingHelper listRouting;
  // Enable Split Horizon
  RipHelper ripRouting;
  ripRouting.Set ("SplitHorizon", EnumValue (RipHelper::SplitHorizonPoisonReverse));
  listRouting.Add (ripRouting, 0);
  stack.SetRoutingHelper (listRouting);
  stack.Install (nodes);

  // Assign IP addresses
  Ipv4AddressHelper address;
  address.SetBase ("10.0.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i0i1 = address.Assign (d0d1);

  address.SetBase ("10.0.2.0", "255.255.255.0");
  Ipv4InterfaceContainer i1i2 = address.Assign (d1d2);

  // Configure RIPv2, add interface addresses
  Ptr<Rip> rip0 = nodes.Get (0)->GetObject<Ipv4> ()->GetRoutingProtocol ()->GetObject<Rip> ();
  Ptr<Rip> rip1 = nodes.Get (1)->GetObject<Ipv4> ()->GetRoutingProtocol ()->GetObject<Rip> ();
  Ptr<Rip> rip2 = nodes.Get (2)->GetObject<Ipv4> ()->GetRoutingProtocol ()->GetObject<Rip> ();

  // Set up static routes for networks directly connected to each node
  rip0->AddNetworkRouteTo (Ipv4Address ("10.0.1.0"), Ipv4Mask ("255.255.255.0"), 1);
  rip1->AddNetworkRouteTo (Ipv4Address ("10.0.1.0"), Ipv4Mask ("255.255.255.0"), 1);
  rip1->AddNetworkRouteTo (Ipv4Address ("10.0.2.0"), Ipv4Mask ("255.255.255.0"), 2);
  rip2->AddNetworkRouteTo (Ipv4Address ("10.0.2.0"), Ipv4Mask ("255.255.255.0"), 1);

  // ICMP applications to generate traffic: ping from Node 0 to Node 2 and from Node 2 to Node 0
  uint16_t port = 9;
  V4PingHelper pingToN2 (i1i2.GetAddress (1));
  pingToN2.SetAttribute ("Verbose", BooleanValue (true));
  ApplicationContainer app0 = pingToN2.Install (nodes.Get (0));
  app0.Start (Seconds (3.0));
  app0.Stop (Seconds (18.0));

  V4PingHelper pingToN0 (i0i1.GetAddress (0));
  pingToN0.SetAttribute ("Verbose", BooleanValue (true));
  ApplicationContainer app2 = pingToN0.Install (nodes.Get (2));
  app2.Start (Seconds (4.0));
  app2.Stop (Seconds (18.0));

  // Enable routing table logging for demonstration
  RipHelper::PrintRoutingTableAllAt (Seconds (5.0), nodes, ns3::Create<OutputStreamWrapper> (&std::cout));

  Simulator::Stop (Seconds (20.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}