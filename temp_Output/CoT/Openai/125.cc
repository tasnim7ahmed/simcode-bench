#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-list-routing-helper.h"
#include "ns3/rip-helper.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("RipStarSimulation");

void
PrintRoutingTable (Ptr<Node> node, Time printTime)
{
  Ipv4RoutingTableEntry entry;
  Ptr<Ipv4> ipv4 = node->GetObject<Ipv4> ();
  std::cout << "Routing table of node " << node->GetId () << " at time " << printTime.GetSeconds () << "s" << std::endl;
  Ptr<Ipv4RoutingProtocol> rp = ipv4->GetRoutingProtocol ();
  rp->PrintRoutingTable (Create<OutputStreamWrapper> (&std::cout));
  std::cout << std::endl;
}

int
main (int argc, char *argv[])
{
  Time::SetResolution (Time::NS);

  NodeContainer centralNode;
  centralNode.Create (1);
  NodeContainer edgeNodes;
  edgeNodes.Create (4);

  NodeContainer allNodes;
  allNodes.Add (centralNode);
  allNodes.Add (edgeNodes);

  std::vector<NodeContainer> starPairs;
  for (uint32_t i = 0; i < edgeNodes.GetN (); ++i)
    {
      NodeContainer pair;
      pair.Add (centralNode.Get (0));
      pair.Add (edgeNodes.Get (i));
      starPairs.push_back (pair);
    }

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

  std::vector<NetDeviceContainer> devicePairs;
  for (uint32_t i = 0; i < starPairs.size (); ++i)
    {
      devicePairs.push_back (p2p.Install (starPairs[i]));
    }

  InternetStackHelper internet;
  RipHelper rip;
  Ipv4ListRoutingHelper listRH;
  listRH.Add (rip, 0);
  internet.SetRoutingHelper (listRH);
  internet.Install (allNodes);

  Ipv4AddressHelper address;
  std::vector<Ipv4InterfaceContainer> ifaces;
  for (uint32_t i = 0; i < devicePairs.size (); ++i)
    {
      std::ostringstream subnet;
      subnet << "10.1." << (i + 1) << ".0";
      address.SetBase (Ipv4Address (subnet.str ().c_str ()), "255.255.255.0");
      ifaces.push_back (address.Assign (devicePairs[i]));
    }

  // Enable tracing
  AsciiTraceHelper ascii;
  p2p.EnableAsciiAll (ascii.CreateFileStream ("rip-star.tr"));
  p2p.EnablePcapAll ("rip-star", true);
  rip.EnableRipAscii (allNodes, ascii.CreateFileStream ("rip-debug.tr"));

  // Schedule printing of routing tables
  for (uint32_t i = 0; i < allNodes.GetN (); ++i)
    {
      Simulator::Schedule (Seconds (2.0), &PrintRoutingTable, allNodes.Get (i), Seconds (2.0));
      Simulator::Schedule (Seconds (8.0), &PrintRoutingTable, allNodes.Get (i), Seconds (8.0));
      Simulator::Schedule (Seconds (12.0), &PrintRoutingTable, allNodes.Get (i), Seconds (12.0));
      Simulator::Schedule (Seconds (16.0), &PrintRoutingTable, allNodes.Get (i), Seconds (16.0));
      Simulator::Schedule (Seconds (20.0), &PrintRoutingTable, allNodes.Get (i), Seconds (20.0));
    }

  Simulator::Stop (Seconds (22.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}