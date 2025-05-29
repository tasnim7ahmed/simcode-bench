/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * IPv6 Fragmentation and PMTU Simulation with Multiple Routers
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv6-address-helper.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/ipv6-routing-table-entry.h"
#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("Ipv6FragmentationPmtuExample");

// Trace sink function
static void
RxTrace (Ptr<const Packet> packet, Ptr<Ipv6> ipv6, uint32_t interface)
{
  static std::ofstream out ("fragmentation-ipv6-PMTU.tr", std::ios::app);

  Time now = Simulator::Now ();
  out << "Time " << now.GetSeconds ()
      << "s Node " << ipv6->GetObject<Node> ()->GetId ()
      << " Ipv6Intf " << interface
      << " Received packet of size " << packet->GetSize ()
      << std::endl;
}

int
main (int argc, char *argv[])
{
  LogComponentEnable ("Ipv6FragmentationPmtuExample", LOG_LEVEL_INFO);
  LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
  LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);

  // --- Node Creation: Src -- r1 -- n1 -- r2 -- n2
  NodeContainer srcNode;
  srcNode.Create (1); // Src

  NodeContainer r1Node;
  r1Node.Create (1);

  NodeContainer n1Node;
  n1Node.Create (1);

  NodeContainer r2Node;
  r2Node.Create (1);

  NodeContainer n2Node;
  n2Node.Create (1);

  // Link definitions for 3 segments:
  // Src <--> r1 (CSMA, MTU=5000)
  // r1  <--> n1 (P2P, MTU=2000)
  // n1  <--> r2 (P2P, MTU=2000)
  // r2  <--> n2 (CSMA, MTU=1500)

  // Containers for each link
  NodeContainer net1 (srcNode.Get(0), r1Node.Get(0));
  NodeContainer net2 (r1Node.Get(0), n1Node.Get(0));
  NodeContainer net3 (n1Node.Get(0), r2Node.Get(0));
  NodeContainer net4 (r2Node.Get(0), n2Node.Get(0));

  // 1. Src-r1 (CSMA, MTU=5000)
  CsmaHelper csma1;
  csma1.SetChannelAttribute ("DataRate", StringValue ("100Mbps"));
  csma1.SetChannelAttribute ("Delay", TimeValue (MilliSeconds(2)));
  csma1.SetDeviceAttribute ("Mtu", UintegerValue (5000));
  NetDeviceContainer dev1 = csma1.Install (net1);

  // 2. r1-n1 (Point-to-Point, MTU=2000)
  PointToPointHelper p2p1;
  p2p1.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  p2p1.SetChannelAttribute ("Delay", TimeValue (MilliSeconds(5)));
  p2p1.SetDeviceAttribute ("Mtu", UintegerValue (2000));
  NetDeviceContainer dev2 = p2p1.Install (net2);

  // 3. n1-r2 (Point-to-Point, MTU=2000)
  PointToPointHelper p2p2;
  p2p2.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  p2p2.SetChannelAttribute ("Delay", TimeValue (MilliSeconds(5)));
  p2p2.SetDeviceAttribute ("Mtu", UintegerValue (2000));
  NetDeviceContainer dev3 = p2p2.Install (net3);

  // 4. r2-n2 (CSMA, MTU=1500)
  CsmaHelper csma2;
  csma2.SetChannelAttribute ("DataRate", StringValue ("100Mbps"));
  csma2.SetChannelAttribute ("Delay", TimeValue (MilliSeconds(2)));
  csma2.SetDeviceAttribute ("Mtu", UintegerValue (1500));
  NetDeviceContainer dev4 = csma2.Install (net4);

  // Aggregate all nodes for Internet stack install
  NodeContainer allNodes;
  allNodes.Add (srcNode);
  allNodes.Add (r1Node);
  allNodes.Add (n1Node);
  allNodes.Add (r2Node);
  allNodes.Add (n2Node);

  // Install Internet Stack
  InternetStackHelper internetv6;
  internetv6.SetIpv4StackInstall (false); // Only IPv6
  internetv6.Install (allNodes);

  // Assign IPv6 addresses
  Ipv6AddressHelper ipv6;
  std::vector<Ipv6InterfaceContainer> ifaces;

  // Net1: Src - r1: 2001:1::/64
  ipv6.SetBase (Ipv6Address ("2001:1::"), Ipv6Prefix (64));
  ifaces.push_back (ipv6.Assign (dev1));
  ifaces.back ().SetForwarding (1, true); // r1
  ifaces.back ().SetDefaultRouteInAllNodes (0); // set src default route

  // Net2: r1 - n1: 2001:2::/64
  ipv6.SetBase (Ipv6Address ("2001:2::"), Ipv6Prefix (64));
  ifaces.push_back (ipv6.Assign (dev2));
  ifaces.back ().SetForwarding (0, true); // r1
  ifaces.back ().SetForwarding (1, true); // n1

  // Net3: n1 - r2: 2001:3::/64
  ipv6.SetBase (Ipv6Address ("2001:3::"), Ipv6Prefix (64));
  ifaces.push_back (ipv6.Assign (dev3));
  ifaces.back ().SetForwarding (0, true); // n1
  ifaces.back ().SetForwarding (1, true); // r2

  // Net4: r2 - n2: 2001:4::/64
  ipv6.SetBase (Ipv6Address ("2001:4::"), Ipv6Prefix (64));
  ifaces.push_back (ipv6.Assign (dev4));
  ifaces.back ().SetForwarding (0, true); // r2
  ifaces.back ().SetForwarding (1, true); // n2
  ifaces.back ().SetDefaultRouteInAllNodes (1); // set n2 default route

  // Manually configure other default/static routes
  Ipv6StaticRoutingHelper routingHelper;

  // Set up routers to forward
  // r1: default :: via n1
  Ptr<Ipv6StaticRouting> r1Static = routingHelper.GetStaticRouting (r1Node.Get (0)->GetObject<Ipv6> ());
  r1Static->SetDefaultRoute (ifaces[1].GetAddress (1, 1), 2);

  // n1: default :: via r2
  Ptr<Ipv6StaticRouting> n1Static = routingHelper.GetStaticRouting (n1Node.Get (0)->GetObject<Ipv6> ());
  n1Static->SetDefaultRoute (ifaces[2].GetAddress (1, 1), 1);

  // r2: default :: via n2
  Ptr<Ipv6StaticRouting> r2Static = routingHelper.GetStaticRouting (r2Node.Get (0)->GetObject<Ipv6> ());
  r2Static->SetDefaultRoute (ifaces[3].GetAddress (1, 1), 1);

  // n2: default :: via r2
  Ptr<Ipv6StaticRouting> n2Static = routingHelper.GetStaticRouting (n2Node.Get (0)->GetObject<Ipv6> ());
  n2Static->SetDefaultRoute (ifaces[3].GetAddress (0, 1), 0);

  // Applications
  // UDP Echo Server on n2
  uint16_t echoPort = 9;
  UdpEchoServerHelper echoServer (echoPort);
  ApplicationContainer serverApps = echoServer.Install (n2Node.Get (0));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  // UDP Echo Client on Src, destined to n2 global (2001:4::2)
  UdpEchoClientHelper echoClient (ifaces[3].GetAddress (1, 1), echoPort);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (1));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  // Set a packet size larger than minimal MTU to force fragmentation (e.g., 4000 bytes)
  echoClient.SetAttribute ("PacketSize", UintegerValue (4000));
  ApplicationContainer clientApps = echoClient.Install (srcNode.Get (0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  // Enable tracing at all devices/interfaces for packet reception
  for (uint32_t nodeIdx = 0; nodeIdx < allNodes.GetN (); ++nodeIdx)
    {
      Ptr<Node> node = allNodes.Get (nodeIdx);
      Ptr<Ipv6> ipv6L3 = node->GetObject<Ipv6> ();
      for (uint32_t i = 0; i < ipv6L3->GetNInterfaces (); ++i)
        {
          std::ostringstream oss;
          oss << "/NodeList/" << node->GetId () << "/DeviceList/" << i << "/$ns3::Ipv6L3Protocol/Rx";
          Config::ConnectWithoutContext (oss.str (), MakeBoundCallback (&RxTrace, ipv6L3, i));
        }
    }

  NS_LOG_INFO ("Starting simulation...");
  Simulator::Stop (Seconds (12.0));
  Simulator::Run ();
  Simulator::Destroy ();
  NS_LOG_INFO ("Simulation finished.");

  return 0;
}