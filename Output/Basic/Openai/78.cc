#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/rip-helper.h"
#include "ns3/ipv4-static-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("RipPoisonReverseExample");

void
PrintRoutingTable (Ptr<Node> node, Time printTime)
{
  Ipv4RoutingHelper* ipv4RoutingHelper = new Ipv4ListRoutingHelper();
  Ipv4StaticRoutingHelper staticRouting;
  RipHelper ripRouting;
  *ipv4RoutingHelper = Ipv4ListRoutingHelper();
  (*static_cast<Ipv4ListRouting*>
    (node->GetObject<Ipv4>()->GetRoutingProtocol ().Get ()))->PrintRoutingTable (Create<OutputStreamWrapper> (&std::cout), Simulator::Now ().GetSeconds ());
  delete ipv4RoutingHelper;
}

int
main (int argc, char **argv)
{
  bool verbose = false;
  bool printRoutingTables = false;
  bool showPings = false;
  CommandLine cmd (__FILE__);
  cmd.AddValue ("verbose", "Enable log components", verbose);
  cmd.AddValue ("printRoutingTables", "Print routing tables at intervals", printRoutingTables);
  cmd.AddValue ("showPings", "Show ping responses", showPings);
  cmd.Parse (argc, argv);

  if (verbose)
    {
      LogComponentEnable ("RipPoisonReverseExample", LOG_LEVEL_INFO);
      LogComponentEnable ("Rip", LOG_LEVEL_ALL);
      LogComponentEnable ("Ipv4Interface", LOG_LEVEL_INFO);
    }

  // Names: 0-SRC, 1-A, 2-B, 3-C, 4-D, 5-DST
  NodeContainer nodes;
  nodes.Create (6);

  Ptr<Node> src = nodes.Get(0);
  Ptr<Node> a   = nodes.Get(1);
  Ptr<Node> b   = nodes.Get(2);
  Ptr<Node> c   = nodes.Get(3);
  Ptr<Node> d   = nodes.Get(4);
  Ptr<Node> dst = nodes.Get(5);

  // Install Internet stack (w/o routing) on all nodes
  InternetStackHelper internet;
  RipHelper ripRouting;
  ripRouting.Set ("SplitHorizon", EnumValue (RipHelper::POISON_REVERSE));
  Ipv4ListRoutingHelper listRH;
  listRH.Add (ripRouting, 0);
  internet.SetRoutingHelper (listRH);
  internet.Install (a);
  internet.Install (b);
  internet.Install (c);
  internet.Install (d);

  // src and dst only need static routing (for default and interface), not RIP
  InternetStackHelper staticInternet;
  Ipv4StaticRoutingHelper staticRouting;
  staticInternet.Install (src);
  staticInternet.Install (dst);

  // Point-to-point helpers
  PointToPointHelper p2p;
  NetDeviceContainer nd01, nd12, nd23, nd34, nd45, nd24;

  // SRC -- A
  p2p.SetDeviceAttribute ("DataRate", StringValue ("1Gbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));
  nd01 = p2p.Install (src, a);

  // A -- B
  nd12 = p2p.Install (a, b);

  // B -- D
  nd23 = p2p.Install (b, d);

  // D -- DST
  nd45 = p2p.Install (d, dst);

  // A -- C
  nd24 = p2p.Install (a, c);

  // C -- D (cost 10)
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));
  NetDeviceContainer nd34;
  nd34 = p2p.Install (c, d);

  // Assign IP addresses
  Ipv4AddressHelper ipv4;
  std::vector<Ipv4InterfaceContainer> interfaces (6);

  // Net 1: SRC<A> 10.0.1.0/24
  ipv4.SetBase ("10.0.1.0", "255.255.255.0");
  interfaces[0] = ipv4.Assign (nd01);

  // Net 2: A<B> 10.0.2.0/24
  ipv4.SetBase ("10.0.2.0", "255.255.255.0");
  interfaces[1] = ipv4.Assign (nd12);

  // Net 3: B<D> 10.0.3.0/24
  ipv4.SetBase ("10.0.3.0", "255.255.255.0");
  interfaces[2] = ipv4.Assign (nd23);

  // Net 4: D<DST> 10.0.4.0/24
  ipv4.SetBase ("10.0.4.0", "255.255.255.0");
  interfaces[3] = ipv4.Assign (nd45);

  // Net 5: A<C> 10.0.5.0/24
  ipv4.SetBase ("10.0.5.0", "255.255.255.0");
  interfaces[4] = ipv4.Assign (nd24);

  // Net 6: C<D> (cost 10) 10.0.6.0/24
  ipv4.SetBase ("10.0.6.0", "255.255.255.0");
  interfaces[5] = ipv4.Assign (nd34);

  // Set link metrics: all 1, except C--D (cost 10)
  // Interface ordering: A(interfaces): [0]=10.0.1.2 [1]=10.0.2.1 [2]=10.0.5.1 ; D: [0]=10.0.3.2 [1]=10.0.4.1 [2]=10.0.6.2
  Ptr<Ipv4> ipv4C = c->GetObject<Ipv4> ();
  Ptr<Ipv4> ipv4D = d->GetObject<Ipv4> ();
  uint32_t ifC = 1; // 10.0.6.1 on C
  uint32_t ifD = 2; // 10.0.6.2 on D
  ripRouting.SetInterfaceMetric (c, ifC, 10);
  ripRouting.SetInterfaceMetric (d, ifD, 10);

  // Default routes for SRC and DST
  Ptr<Ipv4StaticRouting> staticSrc = staticRouting.GetStaticRouting (src->GetObject<Ipv4> ());
  staticSrc->SetDefaultRoute ("10.0.1.2", 1);
  Ptr<Ipv4StaticRouting> staticDst = staticRouting.GetStaticRouting (dst->GetObject<Ipv4> ());
  staticDst->SetDefaultRoute ("10.0.4.1", 1);

  // ICMP Ping from SRC to DST
  uint32_t packetSize = 56;
  uint32_t maxPackets = 25;
  Time interPacketInterval = Seconds (2.0);

  V4PingHelper ping ("10.0.4.2");
  ping.SetAttribute ("Interval", TimeValue (interPacketInterval));
  ping.SetAttribute ("Size", UintegerValue (packetSize));
  ping.SetAttribute ("Count", UintegerValue (maxPackets));

  if (showPings)
    {
      ping.SetAttribute ("Verbose", BooleanValue (true));
    }
  ApplicationContainer apps = ping.Install (src);
  apps.Start (Seconds (6.0));
  apps.Stop (Seconds (60.0));

  // Link failure and recovery: B--D (Net 3)
  Ptr<Node> bNode = b;
  Ptr<Node> dNode = d;
  Ptr<NetDevice> bdOnB = nd23.Get (0);
  Ptr<NetDevice> bdOnD = nd23.Get (1);

  Simulator::Schedule (Seconds (40.0), &NetDevice::SetLinkDown, bdOnB);
  Simulator::Schedule (Seconds (40.0), &NetDevice::SetLinkDown, bdOnD);
  Simulator::Schedule (Seconds (44.0), &NetDevice::SetLinkUp, bdOnB);
  Simulator::Schedule (Seconds (44.0), &NetDevice::SetLinkUp, bdOnD);

  // Print routing tables
  if (printRoutingTables)
    {
      for (double t = 0.0; t <= 60.0; t += 5.0)
        {
          Simulator::Schedule (Seconds (t), &PrintRoutingTable, a, Seconds (t));
          Simulator::Schedule (Seconds (t), &PrintRoutingTable, b, Seconds (t));
          Simulator::Schedule (Seconds (t), &PrintRoutingTable, c, Seconds (t));
          Simulator::Schedule (Seconds (t), &PrintRoutingTable, d, Seconds (t));
        }
    }

  Simulator::Stop (Seconds (65.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}