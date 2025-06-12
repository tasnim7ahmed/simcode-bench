#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/ospf-routing-helper.h"

using namespace ns3;

void PrintRoutingTable (Ptr<Node> node, Time printTime) {
  Ipv4StaticRoutingHelper staticRoutingHelper;
  Ipv4GlobalRoutingHelper globalRoutingHelper;
  OspfRoutingHelper ospf;
  Ptr<Ipv4> ipv4 = node->GetObject<Ipv4> ();
  std::cout << "Routing table for node " << node->GetId () << " at " << printTime.GetSeconds () << "s:" << std::endl;
  ospf.PrintRoutingTable (node);
  std::cout << std::endl;
}

int main (int argc, char *argv[])
{
  Time::SetResolution (Time::NS);

  NodeContainer nodes;
  nodes.Create (4);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer d01 = p2p.Install (nodes.Get(0), nodes.Get(1));
  NetDeviceContainer d12 = p2p.Install (nodes.Get(1), nodes.Get(2));
  NetDeviceContainer d23 = p2p.Install (nodes.Get(2), nodes.Get(3));
  NetDeviceContainer d30 = p2p.Install (nodes.Get(3), nodes.Get(0));

  // Diagonal links for redundancy (optional)
  NetDeviceContainer d02 = p2p.Install (nodes.Get(0), nodes.Get(2));
  NetDeviceContainer d13 = p2p.Install (nodes.Get(1), nodes.Get(3));

  // OSPF must be enabled for routing 
  OspfRoutingHelper ospf;
  InternetStackHelper stack;
  stack.SetRoutingHelper (ospf);
  stack.Install (nodes);

  Ipv4AddressHelper ipv4;
  // Assign subnets to each link
  ipv4.SetBase ("10.0.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i01 = ipv4.Assign (d01);

  ipv4.SetBase ("10.0.2.0", "255.255.255.0");
  Ipv4InterfaceContainer i12 = ipv4.Assign (d12);

  ipv4.SetBase ("10.0.3.0", "255.255.255.0");
  Ipv4InterfaceContainer i23 = ipv4.Assign (d23);

  ipv4.SetBase ("10.0.4.0", "255.255.255.0");
  Ipv4InterfaceContainer i30 = ipv4.Assign (d30);

  ipv4.SetBase ("10.0.5.0", "255.255.255.0");
  Ipv4InterfaceContainer i02 = ipv4.Assign (d02);

  ipv4.SetBase ("10.0.6.0", "255.255.255.0");
  Ipv4InterfaceContainer i13 = ipv4.Assign (d13);

  // Enable OSPF on all links and interfaces (the ns-3 OSPF helper does this by default)

  // Print routing tables at time = 5s when OSPF would have converged
  Simulator::Schedule (Seconds (5.0), &PrintRoutingTable, nodes.Get (0), Seconds (5.0));
  Simulator::Schedule (Seconds (5.0), &PrintRoutingTable, nodes.Get (1), Seconds (5.0));
  Simulator::Schedule (Seconds (5.0), &PrintRoutingTable, nodes.Get (2), Seconds (5.0));
  Simulator::Schedule (Seconds (5.0), &PrintRoutingTable, nodes.Get (3), Seconds (5.0));

  Simulator::Stop (Seconds (6.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}