#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

void
PrintRoutingTable (Ptr<Node> node, Time printTime)
{
  Ipv4StaticRoutingHelper staticRoutingHelper;
  Ptr<Ipv4> ipv4 = node->GetObject<Ipv4> ();
  Ipv4RoutingTableEntry route;
  std::cout << "Routing table for node " << node->GetId () << " at time " << printTime.GetSeconds () << "s\n";
  ipv4->GetRoutingProtocol ()->PrintRoutingTable (Create<OutputStreamWrapper> (&std::cout));
  std::cout << "-------------------------------------------------------" << std::endl;
}

int
main (int argc, char *argv[])
{
  // Command line arguments if any
  CommandLine cmd (__FILE__);
  cmd.Parse (argc, argv);

  // Create nodes
  NodeContainer nodes;
  nodes.Create (4);

  // Define network topology (square)
  // Nodes: 0---1
  //        |   |
  //        3---2

  // Point-to-point links
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

  // Connect links according to the square topology
  NetDeviceContainer d01 = p2p.Install (nodes.Get (0), nodes.Get (1));
  NetDeviceContainer d12 = p2p.Install (nodes.Get (1), nodes.Get (2));
  NetDeviceContainer d23 = p2p.Install (nodes.Get (2), nodes.Get (3));
  NetDeviceContainer d30 = p2p.Install (nodes.Get (3), nodes.Get (0));

  // Optionally connect the cross link to increase path options
  NetDeviceContainer d13 = p2p.Install (nodes.Get (1), nodes.Get (3));
  
  // Install internet stack using OSPF
  OspfHelper ospf;
  InternetStackHelper internet;
  internet.SetRoutingHelper (ospf);
  internet.Install (nodes);

  // Assign IP addresses
  Ipv4AddressHelper ipv4;

  ipv4.SetBase ("10.0.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i01 = ipv4.Assign (d01);

  ipv4.SetBase ("10.0.2.0", "255.255.255.0");
  Ipv4InterfaceContainer i12 = ipv4.Assign (d12);

  ipv4.SetBase ("10.0.3.0", "255.255.255.0");
  Ipv4InterfaceContainer i23 = ipv4.Assign (d23);

  ipv4.SetBase ("10.0.4.0", "255.255.255.0");
  Ipv4InterfaceContainer i30 = ipv4.Assign (d30);

  ipv4.SetBase ("10.0.5.0", "255.255.255.0");
  Ipv4InterfaceContainer i13 = ipv4.Assign (d13);

  // Enable global routing (OSPF will compute routes automatically)
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // Schedule print of routing tables after OSPF converges (~5s)
  Simulator::Schedule (Seconds (5.0), &PrintRoutingTable, nodes.Get (0), Seconds (5.0));
  Simulator::Schedule (Seconds (5.0), &PrintRoutingTable, nodes.Get (1), Seconds (5.0));
  Simulator::Schedule (Seconds (5.0), &PrintRoutingTable, nodes.Get (2), Seconds (5.0));
  Simulator::Schedule (Seconds (5.0), &PrintRoutingTable, nodes.Get (3), Seconds (5.0));

  Simulator::Stop (Seconds (6.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}