#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/rip-helper.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("RipRoutingExample");

void
PrintRoutingTable (Ptr<Node> node, Time printTime)
{
  Ipv4StaticRoutingHelper ipv4RoutingHelper;
  Ptr<Ipv4> ipv4 = node->GetObject<Ipv4> ();
  std::cout << "Routing table of node: " << node->GetId () << " at time " << Simulator::Now ().GetSeconds () << "s" << std::endl;
  Ptr<OutputStreamWrapper> routingStream = Create<OutputStreamWrapper> (&std::cout);
  ipv4->GetRoutingProtocol ()->PrintRoutingTable (routingStream);
}

void
PrintAllRoutingTables (NodeContainer routers, Time printTime, double interval)
{
  for (uint32_t i = 0; i < routers.GetN (); i++)
    {
      PrintRoutingTable (routers.Get (i), printTime);
    }
  Simulator::Schedule (Seconds (interval), &PrintAllRoutingTables, routers, printTime + Seconds (interval), interval);
}

int
main (int argc, char **argv)
{
  bool verbose = false;
  bool printRoutingTables = false;
  bool showPings = false;
  double printInterval = 10.0;

  CommandLine cmd (__FILE__);
  cmd.AddValue ("verbose", "Enable verbose log output", verbose);
  cmd.AddValue ("printRoutingTables", "Print routing tables at intervals", printRoutingTables);
  cmd.AddValue ("showPings", "Show Ping6 application results", showPings);
  cmd.AddValue ("printInterval", "Interval between routing table prints (seconds)", printInterval);
  cmd.Parse (argc, argv);

  if (verbose)
    {
      LogComponentEnable ("RipRoutingExample", LOG_LEVEL_INFO);
      LogComponentEnable ("Rip", LOG_LEVEL_INFO);
      LogComponentEnable ("Ping", LOG_LEVEL_INFO);
    }

  NodeContainer nodes;
  nodes.Create (6); // 0=SRC, 1=A, 2=B, 3=C, 4=D, 5=DST

  Ptr<Node> src = nodes.Get (0);
  Ptr<Node> a   = nodes.Get (1);
  Ptr<Node> b   = nodes.Get (2);
  Ptr<Node> c   = nodes.Get (3);
  Ptr<Node> d   = nodes.Get (4);
  Ptr<Node> dst = nodes.Get (5);

  // Link: SRC<-->A
  NodeContainer link1 (src, a);
  // Link: A<-->B
  NodeContainer link2 (a, b);
  // Link: B<-->C
  NodeContainer link3 (b, c);
  // Link: C<-->D (cost 10)
  NodeContainer link4 (c, d);
  // Link: D<-->DST
  NodeContainer link5 (d, dst);
  // Link: B<-->D
  NodeContainer link6 (b, d);

  PointToPointHelper p2p_cost1;
  p2p_cost1.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  p2p_cost1.SetChannelAttribute ("Delay", StringValue ("2ms"));

  PointToPointHelper p2p_cost10;
  p2p_cost10.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  p2p_cost10.SetChannelAttribute ("Delay", StringValue ("20ms"));

  // NetDevices
  NetDeviceContainer d1 = p2p_cost1.Install (link1);
  NetDeviceContainer d2 = p2p_cost1.Install (link2);
  NetDeviceContainer d3 = p2p_cost1.Install (link3);
  NetDeviceContainer d4 = p2p_cost10.Install (link4); // cost 10
  NetDeviceContainer d5 = p2p_cost1.Install (link5);
  NetDeviceContainer d6 = p2p_cost1.Install (link6);

  // Assign IP addresses per link
  InternetStackHelper stack;
  RipHelper ripRouting;
  ripRouting.ExcludeInterface (src, 1); // do not run RIP on SRC node
  ripRouting.ExcludeInterface (dst, 1); // do not run RIP on DST node
  ripRouting.Set ("SplitHorizon", EnumValue (RipHelper::POISON_REVERSE));

  Ipv4AddressHelper addr;
  std::vector<Ipv4InterfaceContainer> ifaces;

  stack.SetRoutingHelper (ripRouting);
  stack.Install (a);
  stack.Install (b);
  stack.Install (c);
  stack.Install (d);

  InternetStackHelper stackNodes;
  stackNodes.Install (src);
  stackNodes.Install (dst);

  addr.SetBase ("10.0.1.0", "255.255.255.0");
  ifaces.push_back (addr.Assign (d1));
  addr.NewNetwork ();
  ifaces.push_back (addr.Assign (d2));
  addr.NewNetwork ();
  ifaces.push_back (addr.Assign (d3));
  addr.NewNetwork ();
  ifaces.push_back (addr.Assign (d4));
  addr.NewNetwork ();
  ifaces.push_back (addr.Assign (d5));
  addr.NewNetwork ();
  ifaces.push_back (addr.Assign (d6));

  // Set link metrics via Ipv4MetricSenders
  // All point-to-point links' metric are 1 except d4 (C<->D), which is 10
  Ptr<Ipv4> ipv4c = c->GetObject<Ipv4> ();
  Ptr<Ipv4> ipv4d = d->GetObject<Ipv4> ();
  // d4 is the 2nd interface on both c and d (as per order of installation)
  // but can be verified:
  // Interfaces: c: 0-loop, 1-link3, 2-link4; d: 0-loop, 1-link4, 2-link5, 3-link6
  ripRouting.SetInterfaceMetric (c, 2, 10); // C -> D
  ripRouting.SetInterfaceMetric (d, 1, 10); // D -> C

  // Let the routing protocol discover these metrics
  // Connect the link-failure event (bring link B-D down at 40s, up at 44s)
  Ptr<NetDevice> b_iface_bd = d6.Get(0);
  Ptr<NetDevice> d_iface_bd = d6.Get(1);
  Simulator::Schedule (Seconds (40), &NetDevice::SetLinkDown, b_iface_bd);
  Simulator::Schedule (Seconds (40), &NetDevice::SetLinkDown, d_iface_bd);
  Simulator::Schedule (Seconds (44), &NetDevice::SetLinkUp, b_iface_bd);
  Simulator::Schedule (Seconds (44), &NetDevice::SetLinkUp, d_iface_bd);

  // Ping application from SRC to DST
  uint16_t port = 9;
  V4PingHelper pingHelper (ifaces[4].GetAddress (1));
  pingHelper.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  pingHelper.SetAttribute ("Verbose", BooleanValue (showPings));
  ApplicationContainer apps = pingHelper.Install (src);
  apps.Start (Seconds (2.0));
  apps.Stop (Seconds (100.0));

  // Print routing tables at regular intervals
  if (printRoutingTables)
    {
      NodeContainer routers;
      routers.Add (a);
      routers.Add (b);
      routers.Add (c);
      routers.Add (d);

      Simulator::Schedule (Seconds (2.0), &PrintAllRoutingTables, routers, Seconds (2.0), printInterval);
    }

  Simulator::Stop (Seconds (100.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}