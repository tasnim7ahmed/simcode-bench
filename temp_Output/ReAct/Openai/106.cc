#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("Ipv6FragmentationPMTUSimulation");

static void
RxTracer (Ptr<const Packet> packet, const Address &address)
{
  static std::ofstream outFile ("fragmentation-ipv6-PMTU.tr", std::ios_base::app);
  outFile << Simulator::Now ().GetSeconds () << "s RX " << *packet << " at " << address << std::endl;
}

int
main (int argc, char *argv[])
{
  LogComponentEnable ("Ipv6FragmentationPMTUSimulation", LOG_LEVEL_INFO);
  LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
  LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);

  //
  // Nodes: Src --- n0 --- r1 --- n1 --- r2 --- n2
  //           (CSMA) (P2P)   (P2P)   (CSMA)
  // MTU per link:      5000    2000      1500
  //

  // Create nodes
  Ptr<Node> src = CreateObject<Node> ();
  Ptr<Node> n0  = CreateObject<Node> ();
  Ptr<Node> r1  = CreateObject<Node> ();
  Ptr<Node> n1  = CreateObject<Node> ();
  Ptr<Node> r2  = CreateObject<Node> ();
  Ptr<Node> n2  = CreateObject<Node> ();

  NodeContainer csma1Nodes; // Src <-> n0
  csma1Nodes.Add (src);
  csma1Nodes.Add (n0);

  NodeContainer p2p1Nodes; // n0 <-> r1
  p2p1Nodes.Add (n0);
  p2p1Nodes.Add (r1);

  NodeContainer p2p2Nodes; // r1 <-> n1
  p2p2Nodes.Add (r1);
  p2p2Nodes.Add (n1);

  NodeContainer csma2Nodes; // n1 <-> r2 <-> n2
  csma2Nodes.Add (n1);
  csma2Nodes.Add (r2);
  csma2Nodes.Add (n2);

  // Install Internet stacks (IPv6)
  InternetStackHelper inet;
  inet.SetIpv4StackInstall (false);
  inet.Install (src);
  inet.Install (n0);
  inet.Install (r1);
  inet.Install (n1);
  inet.Install (r2);
  inet.Install (n2);

  // CSMA 1: Src <-> n0 (MTU 5000)
  CsmaHelper csma1;
  csma1.SetChannelAttribute ("DataRate", DataRateValue (DataRate (10000000)));
  csma1.SetChannelAttribute ("Delay", TimeValue (NanoSeconds (6560)));
  csma1.SetDeviceAttribute ("Mtu", UintegerValue (5000));
  NetDeviceContainer csma1Devs = csma1.Install (csma1Nodes);

  // Point-to-Point 1: n0 <-> r1 (MTU 2000)
  PointToPointHelper p2p1;
  p2p1.SetDeviceAttribute ("DataRate", StringValue ("20Mbps"));
  p2p1.SetChannelAttribute ("Delay", StringValue ("2ms"));
  p2p1.SetDeviceAttribute ("Mtu", UintegerValue (2000));
  NetDeviceContainer p2p1Devs = p2p1.Install (p2p1Nodes);

  // Point-to-Point 2: r1 <-> n1 (MTU 2000)
  PointToPointHelper p2p2;
  p2p2.SetDeviceAttribute ("DataRate", StringValue ("20Mbps"));
  p2p2.SetChannelAttribute ("Delay", StringValue ("2ms"));
  p2p2.SetDeviceAttribute ("Mtu", UintegerValue (2000));
  NetDeviceContainer p2p2Devs = p2p2.Install (p2p2Nodes);

  // CSMA 2: n1 <-> r2 <-> n2 (MTU 1500)
  CsmaHelper csma2;
  csma2.SetChannelAttribute ("DataRate", DataRateValue (DataRate (10000000)));
  csma2.SetChannelAttribute ("Delay", TimeValue (NanoSeconds (6560)));
  csma2.SetDeviceAttribute ("Mtu", UintegerValue (1500));
  NetDeviceContainer csma2Devs = csma2.Install (csma2Nodes);

  // Assign IPv6 addresses
  Ipv6AddressHelper ipv6;
  Ipv6InterfaceContainer iif1, iif2, iif3, iif4;

  // csma1: 2001:1::/64 for Src <--> n0
  ipv6.SetBase (Ipv6Address ("2001:1::"), Ipv6Prefix (64));
  iif1 = ipv6.Assign (csma1Devs);
  iif1.SetForwarding (1, true); // n0 forwards

  // p2p1: 2001:2::/64 for n0 <-> r1
  ipv6.SetBase (Ipv6Address ("2001:2::"), Ipv6Prefix (64));
  iif2 = ipv6.Assign (p2p1Devs);
  iif2.SetForwarding (0, true); // n0
  iif2.SetForwarding (1, true); // r1

  // p2p2: 2001:3::/64 for r1 <-> n1
  ipv6.SetBase (Ipv6Address ("2001:3::"), Ipv6Prefix (64));
  iif3 = ipv6.Assign (p2p2Devs);
  iif3.SetForwarding (0, true); // r1
  iif3.SetForwarding (1, true); // n1

  // csma2: 2001:4::/64 for n1 <-> r2 <-> n2
  ipv6.SetBase (Ipv6Address ("2001:4::"), Ipv6Prefix (64));
  iif4 = ipv6.Assign (csma2Devs);
  iif4.SetForwarding (0, true); // n1
  iif4.SetForwarding (1, true); // r2
  iif4.SetForwarding (2, false); // n2

  // Activate global routing
  Ipv6StaticRoutingHelper ipv6RoutingHelper;

  // Src default route -> n0 (csma1) 
  Ptr<Ipv6StaticRouting> srcStaticRouting = ipv6RoutingHelper.GetStaticRouting (src->GetObject<Ipv6> ());
  srcStaticRouting->SetDefaultRoute (iif1.GetAddress (1, 1), 1);

  // n0 default route -> r1 (p2p1)
  Ptr<Ipv6StaticRouting> n0StaticRouting = ipv6RoutingHelper.GetStaticRouting (n0->GetObject<Ipv6> ());
  n0StaticRouting->SetDefaultRoute (iif2.GetAddress (1, 1), 2);

  // r1 routes: 2001:4::/64 (csma2 subnet) via n1; 2001:1::/64 via n0
  Ptr<Ipv6StaticRouting> r1StaticRouting = ipv6RoutingHelper.GetStaticRouting (r1->GetObject<Ipv6> ());
  r1StaticRouting->AddNetworkRouteTo (Ipv6Address ("2001:1::"), 64, iif2.GetAddress (0, 1), 1);
  r1StaticRouting->AddNetworkRouteTo (Ipv6Address ("2001:4::"), 64, iif3.GetAddress (1, 1), 2);
  r1StaticRouting->SetDefaultRoute (iif3.GetAddress (1, 1), 2);

  // n1 default route -> r2 (csma2)
  Ptr<Ipv6StaticRouting> n1StaticRouting = ipv6RoutingHelper.GetStaticRouting (n1->GetObject<Ipv6> ());
  n1StaticRouting->SetDefaultRoute (iif4.GetAddress (1, 1), 3);

  // r2 routes: 2001:1::/64 via n1
  Ptr<Ipv6StaticRouting> r2StaticRouting = ipv6RoutingHelper.GetStaticRouting (r2->GetObject<Ipv6> ());
  r2StaticRouting->AddNetworkRouteTo (Ipv6Address ("2001:1::"), 64, iif4.GetAddress (0, 1), 1);
  r2StaticRouting->AddNetworkRouteTo (Ipv6Address ("2001:2::"), 64, iif4.GetAddress (0, 1), 1);
  r2StaticRouting->AddNetworkRouteTo (Ipv6Address ("2001:3::"), 64, iif4.GetAddress (0, 1), 1);
  r2StaticRouting->SetDefaultRoute (iif4.GetAddress (2, 1), 2);

  // n2: node at edge
  Ptr<Ipv6StaticRouting> n2StaticRouting = ipv6RoutingHelper.GetStaticRouting (n2->GetObject<Ipv6> ());
  n2StaticRouting->SetDefaultRoute (iif4.GetAddress (1, 1), 1);

  // Install UDP Echo Server on n2
  uint16_t echoPort = 9;
  UdpEchoServerHelper echoServer (echoPort);
  ApplicationContainer serverApps = echoServer.Install (n2);
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  // Install UDP Echo Client on Src, send oversized (4500 bytes) to trigger fragmentation
  UdpEchoClientHelper echoClient (iif4.GetAddress (2, 1), echoPort);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (3));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (4500));
  ApplicationContainer clientApps = echoClient.Install (src);
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  // Enable packet tracing on n2
  Config::ConnectWithoutContext ("/NodeList/" + std::to_string (n2->GetId ()) + "/DeviceList/*/MacRx", MakeCallback (&RxTracer));

  // Enable pcap on all devices
  csma1.EnablePcapAll ("fragmentation-ipv6-csma1", true);
  p2p1.EnablePcapAll ("fragmentation-ipv6-p2p1", true);
  p2p2.EnablePcapAll ("fragmentation-ipv6-p2p2", true);
  csma2.EnablePcapAll ("fragmentation-ipv6-csma2", true);

  Simulator::Stop (Seconds (11.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}