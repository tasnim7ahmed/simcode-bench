#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-list-routing-helper.h"
#include "ns3/ipv4-routing-table-entry.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/rip-helper.h"
#include "ns3/netanim-module.h"

using namespace ns3;

void
PrintRoutingTable (Ptr<Node> node, Time printTime)
{
  Ipv4StaticRoutingHelper staticRouting;
  Ptr<Ipv4> ipv4 = node->GetObject<Ipv4> ();
  Ptr<Ipv4RoutingProtocol> proto = ipv4->GetRoutingProtocol ();
  NS_LOG_UNCOND ("Routing table of node " << node->GetId () <<
                 " (" << Simulator::Now ().GetSeconds () << "s):");
  proto->PrintRoutingTable (Create<OutputStreamWrapper> (&std::cout), Time::Now ());
}

int
main (int argc, char *argv[])
{
  Time::SetResolution (Time::NS);
  LogComponentEnable ("Rip", LOG_LEVEL_INFO);

  NodeContainer routers;
  routers.Create (3); // 0: A, 1: B, 2: C

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("1Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer ndcAB = p2p.Install (routers.Get (0), routers.Get (1));
  NetDeviceContainer ndcBC = p2p.Install (routers.Get (1), routers.Get (2));
  NetDeviceContainer ndcCA = p2p.Install (routers.Get (2), routers.Get (0)); // Triangle

  InternetStackHelper stack;
  RipHelper ripRouting;
  Ipv4ListRoutingHelper list;
  list.Add (ripRouting, 0);
  list.Add (Ipv4StaticRoutingHelper (), 10);
  stack.SetRoutingHelper (list);
  stack.Install (routers);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.0.1.0", "255.255.255.0");
  Ipv4InterfaceContainer ifAB = ipv4.Assign (ndcAB);

  ipv4.SetBase ("10.0.2.0", "255.255.255.0");
  Ipv4InterfaceContainer ifBC = ipv4.Assign (ndcBC);

  ipv4.SetBase ("10.0.3.0", "255.255.255.0");
  Ipv4InterfaceContainer ifCA = ipv4.Assign (ndcCA);

  // Enable RIP on all interfaces except loopback
  ripRouting.ExcludeInterface (routers.Get (0), 1); // If0: loopback
  ripRouting.ExcludeInterface (routers.Get (1), 1);
  ripRouting.ExcludeInterface (routers.Get (2), 1);

  // Start routing
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // Print initial routing tables
  Simulator::Schedule (Seconds (2.0), &PrintRoutingTable, routers.Get(0), Seconds(2.0));
  Simulator::Schedule (Seconds (2.0), &PrintRoutingTable, routers.Get(1), Seconds(2.0));
  Simulator::Schedule (Seconds (2.0), &PrintRoutingTable, routers.Get(2), Seconds(2.0));

  // Break B-C link at t=5s
  Simulator::Schedule (Seconds (5.0), [&] {
      ndcBC.Get (0)->SetDown ();
      ndcBC.Get (1)->SetDown ();
      NS_LOG_UNCOND ("**** Link between B and C is down at " << Simulator::Now ().GetSeconds () << "s ****");
    });

  // Observe after the link break
  for (double t = 6.0; t <= 60.0; t += 5.0)
    {
      Simulator::Schedule (Seconds (t), &PrintRoutingTable, routers.Get(0), Seconds(t));
      Simulator::Schedule (Seconds (t), &PrintRoutingTable, routers.Get(1), Seconds(t));
      Simulator::Schedule (Seconds (t), &PrintRoutingTable, routers.Get(2), Seconds(t));
    }

  Simulator::Stop (Seconds (65.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}