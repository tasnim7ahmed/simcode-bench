#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-list-routing-helper.h"
#include "ns3/ipv4-routing-table-entry.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/ipv4-nix-vector-helper.h"
#include "ns3/ipv4-interface-container.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/distance-vector-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("CountToInfinityDemo");

void PrintRoutingTable (Ptr<Node> node, Time printTime)
{
  Ptr<Ipv4> ipv4 = node->GetObject<Ipv4> ();
  Ipv4RoutingTableEntry entry;
  Ptr<Ipv4RoutingProtocol> proto = ipv4->GetRoutingProtocol ();
  std::ostringstream os;
  proto->PrintRoutingTable (os);
  std::cout << "\n[Time " << printTime.GetSeconds () << "s] Routing table of node " 
            << node->GetId () << ":\n" << os.str () << std::endl;
}

int main (int argc, char *argv[])
{
  LogComponentEnable ("CountToInfinityDemo", LOG_LEVEL_INFO);

  Config::SetDefault ("ns3::DistanceVectorRoutingProtocol::TriggeredUpdates", BooleanValue (true));
  Config::SetDefault ("ns3::DistanceVectorRoutingProtocol::SplitHorizon", BooleanValue (false));
  Config::SetDefault ("ns3::DistanceVectorRoutingProtocol::PoisonReverse", BooleanValue (false));
  Config::SetDefault ("ns3::DistanceVectorRoutingProtocol::Infinity", UintegerValue (16));
  Config::SetDefault ("ns3::DistanceVectorRoutingProtocol::UpdateInterval", TimeValue (Seconds (2)));
  Config::SetDefault ("ns3::DistanceVectorRoutingProtocol::GarbageCollectionInterval", TimeValue (Seconds (30)));
  Config::SetDefault ("ns3::DistanceVectorRoutingProtocol::HoldDownTime", TimeValue (Seconds (100)));

  NodeContainer nodes;
  nodes.Create (3); // 0:A, 1:B, 2:C

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer devAB = p2p.Install (nodes.Get (0), nodes.Get (1));
  NetDeviceContainer devBC = p2p.Install (nodes.Get (1), nodes.Get (2));
  NetDeviceContainer devCA = p2p.Install (nodes.Get (2), nodes.Get (0));

  InternetStackHelper stack;
  DistanceVectorRoutingHelper dv;
  Ipv4ListRoutingHelper list;
  list.Add (dv, 10);
  stack.SetRoutingHelper (list);
  stack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer ifaceAB = address.Assign (devAB);

  address.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer ifaceBC = address.Assign (devBC);

  address.SetBase ("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer ifaceCA = address.Assign (devCA);

  Simulator::Schedule (Seconds (1), &PrintRoutingTable, nodes.Get (0), Seconds (1));
  Simulator::Schedule (Seconds (1), &PrintRoutingTable, nodes.Get (1), Seconds (1));
  Simulator::Schedule (Seconds (1), &PrintRoutingTable, nodes.Get (2), Seconds (1));

  Simulator::Schedule (Seconds (5), &PrintRoutingTable, nodes.Get (0), Seconds (5));
  Simulator::Schedule (Seconds (5), &PrintRoutingTable, nodes.Get (1), Seconds (5));
  Simulator::Schedule (Seconds (5), &PrintRoutingTable, nodes.Get (2), Seconds (5));

  // Simulate link failure between B and C at 6 seconds
  Simulator::Schedule (Seconds (6), [&]() {
      devBC.Get (0)->SetLinkDown ();
      devBC.Get (1)->SetLinkDown ();
      NS_LOG_INFO ("*** Link between B and C went DOWN at 6s ***");
    });

  for (int t = 10; t <= 50; t += 10)
    {
      Simulator::Schedule (Seconds (t), &PrintRoutingTable, nodes.Get (0), Seconds (t));
      Simulator::Schedule (Seconds (t), &PrintRoutingTable, nodes.Get (1), Seconds (t));
      Simulator::Schedule (Seconds (t), &PrintRoutingTable, nodes.Get (2), Seconds (t));
    }

  Simulator::Stop (Seconds (55));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}