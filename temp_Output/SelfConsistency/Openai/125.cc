#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-routing-table-entry.h"
#include "ns3/ipv4-list-routing-helper.h"
#include "ns3/rip-helper.h"
#include <iostream>
#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("RipStarTopologyExample");

// Trace function to dump routing tables at events
void
PrintRoutingTable (Ptr<Node> node, Time printTime)
{
  Ipv4StaticRoutingHelper staticRoutingHelper;
  Ipv4RoutingTableEntry route;
  std::ostringstream oss;
  Ptr<Ipv4> ipv4 = node->GetObject<Ipv4> ();

  oss << "Node: " << node->GetId () << " Routing table at " << printTime.GetSeconds () << "s:\n";
  for (uint32_t j = 0; j < ipv4->GetNInterfaces (); ++j)
    {
      uint32_t nRoutes = ipv4->GetRoutingProtocol ()->GetNRoutes ();
      for (uint32_t i = 0; i < nRoutes; ++i)
        {
          route = ipv4->GetRoutingProtocol ()->GetRoute (i);
          oss << route.GetDestNetwork () << "/" << (int) route.GetDestNetworkMask ().CountOnes ()
              << " via " << route.GetGateway () << " dev " << route.GetInterface () << "\n";
        }
    }

  std::ofstream out ("routing-tables.txt", std::ios::app);
  out << oss.str () << std::endl;
  out.close ();
}

int
main (int argc, char *argv[])
{
  // Enable logging
  LogComponentEnable ("RipStarTopologyExample", LOG_LEVEL_INFO);

  // Set simulation parameters
  uint32_t numEdgeNodes = 4;
  double simTime = 60.0; // seconds

  // Create central (main router) and edge (subnet routers) nodes
  NodeContainer centralNode;
  centralNode.Create (1);
  NodeContainer edgeNodes;
  edgeNodes.Create (numEdgeNodes);

  // For CSMA links between central and each edge node
  std::vector<NodeContainer> starLinks;

  // Keep NetDeviceContainers for later address assignment
  std::vector<NetDeviceContainer> deviceContainers;

  // Create separate CSMA channels between the central router and each edge node
  for (uint32_t i = 0; i < numEdgeNodes; ++i)
    {
      NodeContainer link (centralNode.Get (0), edgeNodes.Get (i));
      starLinks.push_back (link);

      CsmaHelper csma;
      csma.SetChannelAttribute ("DataRate", StringValue ("100Mbps"));
      csma.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (2)));
      NetDeviceContainer devices = csma.Install (link);
      deviceContainers.push_back (devices);
    }

  // Install the Internet stack with RIP
  RipHelper ripRouting;
  Ipv4ListRoutingHelper listRH;
  Ipv4StaticRoutingHelper staticRh;
  // add RIP first (lowest priority routes are inserted last)
  listRH.Add (ripRouting, 0);
  listRH.Add (staticRh, 10);

  InternetStackHelper internetStack;
  internetStack.SetRoutingHelper (listRH);
  //install stack to all nodes
  NodeContainer allNodes;
  allNodes.Add (centralNode);
  allNodes.Add (edgeNodes);
  internetStack.Install (allNodes);

  // Assign IP addresses for each point-to-point (well, csma) link
  Ipv4AddressHelper address;
  std::vector<Ipv4InterfaceContainer> interfaceContainers;
  for (uint32_t i = 0; i < numEdgeNodes; ++i)
    {
      std::ostringstream subnet;
      subnet << "10.1." << i+1 << ".0";
      address.SetBase (Ipv4Address (subnet.str ().c_str ()), "255.255.255.0");
      Ipv4InterfaceContainer interfaces = address.Assign (deviceContainers[i]);
      interfaceContainers.push_back (interfaces);
    }

  // Enable RIP on all interfaces except loopback
  // (Nothing else to do: RIP will listen on those links by default)

  // Schedule periodic routing table dumps
  for (uint32_t n = 0; n < allNodes.GetN (); ++n)
    {
      for (double t = 0; t <= simTime; t += 10.0)
        {
          Simulator::Schedule (Seconds (t), &PrintRoutingTable, allNodes.Get (n), Seconds (t));
        }
    }

  // Enable pcap tracing on all CSMA devices
  for (uint32_t i = 0; i < deviceContainers.size (); ++i)
    {
      CsmaHelper csma;
      csma.EnablePcapAll ("rip-star-topology", false);
    }

  // Run Simulation
  Simulator::Stop (Seconds (simTime));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}