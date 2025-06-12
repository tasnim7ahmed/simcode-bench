#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/csma-module.h"
#include "ns3/point-to-point-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("RipStarTopology");

void
PrintRoutingTable (Ptr<Node> node, Time printTime)
{
  Ipv4StaticRoutingHelper staticRouting;
  Ptr<Ipv4> ipv4 = node->GetObject<Ipv4> ();
  std::ostringstream oss;
  oss << "Time: " << Simulator::Now ().GetSeconds ()
      << "s, Node " << node->GetId () << " Routing Table:";
  NS_LOG_UNCOND (oss.str ());
  Ipv4RoutingTableEntry route;
  for (uint32_t j = 0; j < ipv4->GetRoutingProtocol ()->GetNRoutes (); ++j)
    {
      route = ipv4->GetRoutingProtocol ()->GetRoute (j);
      NS_LOG_UNCOND (route.ToString (node->GetObject<Ipv4> ()));
    }
  if (Simulator::Now () + printTime < Seconds (30.0))
    {
      Simulator::Schedule (printTime, &PrintRoutingTable, node, printTime);
    }
}

int
main (int argc, char *argv[])
{
  Time::SetResolution (Time::NS);

  // Enable logging
  LogComponentEnable ("RipStarTopology", LOG_LEVEL_INFO);

  // Create 5 nodes: 0-central, 1-4 edges
  NodeContainer centralNode;
  centralNode.Create (1);

  NodeContainer edgeNodes;
  edgeNodes.Create (4);

  NodeContainer allNodes;
  allNodes.Add (centralNode);
  allNodes.Add (edgeNodes);

  // Point-to-point links between center and each edge node
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer devices[4];
  for (uint32_t i = 0; i < 4; ++i)
    {
      devices[i] = p2p.Install (NodeContainer (centralNode.Get (0), edgeNodes.Get (i)));
    }

  // Install Internet stack with RIP on all nodes
  RipHelper ripRoutingHelper;
  Ipv4ListRoutingHelper listRH;
  listRH.Add (ripRoutingHelper, 0);

  InternetStackHelper stack;
  stack.SetRoutingHelper (listRH);
  stack.Install (allNodes);

  // Assign IP addresses
  Ipv4AddressHelper address;
  std::vector<Ipv4InterfaceContainer> interfaceContainers (4);

  for (uint32_t i = 0; i < 4; ++i)
    {
      std::ostringstream subnet;
      subnet << "10.1." << (i + 1) << ".0";
      address.SetBase (Ipv4Address (subnet.str ().c_str ()), "255.255.255.0");
      interfaceContainers[i] = address.Assign (devices[i]);
    }

  // Set interface metric 1 for all devices
  for (uint32_t i = 0; i < 4; ++i)
    {
      Ptr<Ipv4> ipv4Central = centralNode.Get (0)->GetObject<Ipv4> ();
      Ptr<Ipv4> ipv4Edge = edgeNodes.Get (i)->GetObject<Ipv4> ();
      ipv4Central->SetMetric (i + 1, 1);
      ipv4Edge->SetMetric (1, 1);
    }

  // Enable global routing (not used, but possible future expansion)
  // Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // Schedule periodic routing table printing
  for (uint32_t i = 0; i < 5; ++i)
    {
      Ptr<Node> node = allNodes.Get (i);
      Simulator::Schedule (Seconds (5.0), &PrintRoutingTable, node, Seconds (5.0));
    }

  Simulator::Stop (Seconds (30.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}