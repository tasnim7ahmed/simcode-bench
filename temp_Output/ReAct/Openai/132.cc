#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-list-routing-helper.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("DvTwoNodeLoop");

void
PrintRoutingTable (Ptr<Node> node, Time printTime)
{
  Ptr<Ipv4> ipv4 = node->GetObject<Ipv4> ();
  Ipv4StaticRoutingHelper ipv4RoutingHelper;
  Ptr<Ipv4StaticRouting> staticRouting = ipv4RoutingHelper.GetStaticRouting (ipv4);
  std::cout << "Routing table of node " << node->GetId()
            << " at " << printTime.GetSeconds() << " s:\n";
  staticRouting->PrintRoutingTable (Create<OutputStreamWrapper>(&std::cout));
}

void
IntroduceLoop (Ptr<Node> nodeA, Ptr<Node> nodeB, Ipv4Address dst, uint32_t ifA, uint32_t ifB)
{
  Ptr<Ipv4> ipv4A = nodeA->GetObject<Ipv4> ();
  Ptr<Ipv4> ipv4B = nodeB->GetObject<Ipv4> ();

  Ipv4StaticRoutingHelper ipv4RoutingHelper;

  Ptr<Ipv4StaticRouting> staticRoutingA = ipv4RoutingHelper.GetStaticRouting (ipv4A);
  Ptr<Ipv4StaticRouting> staticRoutingB = ipv4RoutingHelper.GetStaticRouting (ipv4B);

  // Remove the correct routes (simulate a broken route to the destination)
  while (staticRoutingA->RemoveRoute (dst)) {}
  while (staticRoutingB->RemoveRoute (dst)) {}

  // Each node now incorrectly believes the other knows a route to the destination
  staticRoutingA->AddHostRouteTo (dst, ipv4B->GetAddress (ifB, 0).GetLocal (), ifA);
  staticRoutingB->AddHostRouteTo (dst, ipv4A->GetAddress (ifA, 0).GetLocal (), ifB);

  std::cout << "*** Two-node loop induced at " << Simulator::Now ().GetSeconds() << " s ***\n";
}

int
main (int argc, char *argv[])
{
  LogComponentEnable("DvTwoNodeLoop", LOG_LEVEL_INFO);
  LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
  LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create(2); // nodeA (0) and nodeB (1)

  NodeContainer dstNode;
  dstNode.Create (1); // destination node (2)

  // Point-to-point link between A and B
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("2ms"));

  NetDeviceContainer devAB = p2p.Install(nodes);

  // Point-to-point links from A and B to dst
  NetDeviceContainer devA_dst = p2p.Install(nodes.Get(0), dstNode.Get(0));
  NetDeviceContainer devB_dst = p2p.Install(nodes.Get(1), dstNode.Get(0));

  // Install Internet stack with static routing only (simulate DV: manually manipulate routes)
  InternetStackHelper internet;
  Ipv4StaticRoutingHelper staticRouting;
  Ipv4ListRoutingHelper listRouting;
  listRouting.Add(staticRouting, 1);
  internet.SetRoutingHelper(listRouting);
  internet.Install(nodes);
  internet.Install(dstNode);

  // Assign IP addresses
  Ipv4AddressHelper ipv4;

  ipv4.SetBase ("10.0.1.0", "255.255.255.0");
  Ipv4InterfaceContainer iAB = ipv4.Assign(devAB);

  ipv4.SetBase ("10.0.2.0", "255.255.255.0");
  Ipv4InterfaceContainer iA_dst = ipv4.Assign(devA_dst);

  ipv4.SetBase ("10.0.3.0", "255.255.255.0");
  Ipv4InterfaceContainer iB_dst = ipv4.Assign(devB_dst);

  // Assign addresses:
  // nodeA: 10.0.1.1 (to B), 10.0.2.1 (to dst)
  // nodeB: 10.0.1.2 (to A), 10.0.3.1 (to dst)
  // dst:   10.0.2.2 (to A), 10.0.3.2 (to B)

  Ptr<Node> nodeA = nodes.Get (0);
  Ptr<Node> nodeB = nodes.Get (1);
  Ptr<Node> nodeDst = dstNode.Get (0);

  Ipv4Address addrDst_A = iA_dst.GetAddress(1); // 10.0.2.2
  Ipv4Address addrDst_B = iB_dst.GetAddress(1); // 10.0.3.2

  // Install static routes for initial DV convergence
  Ptr<Ipv4> ipv4A = nodeA->GetObject<Ipv4> ();
  Ptr<Ipv4> ipv4B = nodeB->GetObject<Ipv4> ();
  Ptr<Ipv4> ipv4Dst = nodeDst->GetObject<Ipv4> ();

  Ptr<Ipv4StaticRouting> srtA = staticRouting.GetStaticRouting(ipv4A);
  Ptr<Ipv4StaticRouting> srtB = staticRouting.GetStaticRouting(ipv4B);
  Ptr<Ipv4StaticRouting> srtDst = staticRouting.GetStaticRouting(ipv4Dst);

  // Add direct route from A to dst
  srtA->AddHostRouteTo(addrDst_A, iA_dst.GetAddress(0), 1);

  // Add direct route from B to dst
  srtB->AddHostRouteTo(addrDst_B, iB_dst.GetAddress(0), 1);

  // Add default routes for dst node to reply
  srtDst->AddHostRouteTo (iA_dst.GetAddress(0), iA_dst.GetAddress(1), 0);
  srtDst->AddHostRouteTo (iB_dst.GetAddress(0), iB_dst.GetAddress(1), 1);

  // Add indirect route for A to reach 10.0.3.2 via B
  srtA->AddHostRouteTo(addrDst_B, iAB.GetAddress(1), 0);
  // Add indirect route for B to reach 10.0.2.2 via A
  srtB->AddHostRouteTo(addrDst_A, iAB.GetAddress(0), 0);

  // Install UDP Echo Server on dst node (listening on port 9)
  UdpEchoServerHelper echoServer (9);
  ApplicationContainer serverApps = echoServer.Install (nodeDst);
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  // Install UDP Echo Client on nodeA, sending to dst via direct link first
  UdpEchoClientHelper echoClient (addrDst_A, 9);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (5));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (64));
  ApplicationContainer clientApps = echoClient.Install (nodeA);
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  // Print routing tables at simulation start and after loop induction
  Simulator::Schedule (Seconds (1.5), &PrintRoutingTable, nodeA, Seconds (1.5));
  Simulator::Schedule (Seconds (1.5), &PrintRoutingTable, nodeB, Seconds (1.5));

  // After 4 seconds, simulate destination unreachable by removing both A-dst and B-dst links from both routing tables and induce the loop
  Simulator::Schedule (Seconds (4.0), &IntroduceLoop, nodeA, nodeB, addrDst_A, 0, 0);

  // Print routing tables after inducing loop
  Simulator::Schedule (Seconds (4.2), &PrintRoutingTable, nodeA, Seconds (4.2));
  Simulator::Schedule (Seconds (4.2), &PrintRoutingTable, nodeB, Seconds (4.2));

  // Enable pcap tracing to see the loop
  p2p.EnablePcapAll ("dv-two-node-loop", true);

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}