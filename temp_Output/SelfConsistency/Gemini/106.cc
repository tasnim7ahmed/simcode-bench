#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/ipv6-routing-table-entry.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("Ipv6FragmentationExample");

int main (int argc, char *argv[])
{
  bool verbose = false;
  bool tracing = false;

  CommandLine cmd;
  cmd.AddValue ("verbose", "Tell application containers to log if set to true.", verbose);
  cmd.AddValue ("tracing", "Enable pcap tracing.", tracing);

  cmd.Parse (argc,argv);

  if (verbose)
    {
      LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
      LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);
    }

  NS_LOG_INFO ("Create nodes.");
  NodeContainer nodes;
  nodes.Create (6);
  Node *src = nodes.Get (0);
  Node *n0 = nodes.Get (1);
  Node *r1 = nodes.Get (2);
  Node *n1 = nodes.Get (3);
  Node *r2 = nodes.Get (4);
  Node *n2 = nodes.Get (5);

  NS_LOG_INFO ("Create channels.");
  PointToPointHelper p2p5000;
  p2p5000.SetDeviceAttribute ("Mtu", UintegerValue (5000));
  NetDeviceContainer src_n0 = p2p5000.Install (src, n0);

  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", StringValue ("5Mbps"));
  csma.SetChannelAttribute ("Delay", TimeValue (NanoSeconds (6560)));

  NetDeviceContainer n0_r1 = csma.Install (NodeContainer (n0, r1));

  PointToPointHelper p2p2000;
  p2p2000.SetDeviceAttribute ("Mtu", UintegerValue (2000));
  NetDeviceContainer r1_n1 = p2p2000.Install (r1, n1);

  CsmaHelper csma2;
  csma2.SetChannelAttribute ("DataRate", StringValue ("5Mbps"));
  csma2.SetChannelAttribute ("Delay", TimeValue (NanoSeconds (6560)));

  NetDeviceContainer n1_r2 = csma2.Install (NodeContainer (n1, r2));

  PointToPointHelper p2p1500;
  p2p1500.SetDeviceAttribute ("Mtu", UintegerValue (1500));
  NetDeviceContainer r2_n2 = p2p1500.Install (r2, n2);

  NS_LOG_INFO ("Install Internet Stack.");
  InternetStackHelper internetv6;
  internetv6.Install (nodes);

  NS_LOG_INFO ("Assign IPv6 Addresses.");
  Ipv6AddressHelper ipv6;
  ipv6.SetBase (Ipv6Address ("2001:db8:0:1::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer i0i1 = ipv6.Assign (src_n0);
  ipv6.SetBase (Ipv6Address ("2001:db8:0:2::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer i1i2 = ipv6.Assign (n0_r1);
  ipv6.SetBase (Ipv6Address ("2001:db8:0:3::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer i2i3 = ipv6.Assign (r1_n1);
  ipv6.SetBase (Ipv6Address ("2001:db8:0:4::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer i3i4 = ipv6.Assign (n1_r2);
  ipv6.SetBase (Ipv6Address ("2001:db8:0:5::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer i4i5 = ipv6.Assign (r2_n2);

  // Set IPv6 global address on the interfaces
  i0i1.SetForwarding (0, true);
  i1i2.SetForwarding (0, true);
  i1i2.SetForwarding (1, true);
  i2i3.SetForwarding (0, true);
  i2i3.SetForwarding (1, true);
  i3i4.SetForwarding (0, true);
  i3i4.SetForwarding (1, true);
  i4i5.SetForwarding (0, true);
  i4i5.SetForwarding (1, true);

  // Populate routing tables
  Ipv6StaticRoutingHelper ipv6RoutingHelper;
  auto staticRoutingSrc = ipv6RoutingHelper.GetStaticRouting (src->GetObject<Ipv6> ());
  staticRoutingSrc->AddRoute (Ipv6Address ("2001:db8:0:2::/64"), 1, i0i1.GetInterfaceIndex (0));
  staticRoutingSrc->AddRoute (Ipv6Address ("2001:db8:0:3::/64"), 1, i0i1.GetInterfaceIndex (0));
  staticRoutingSrc->AddRoute (Ipv6Address ("2001:db8:0:4::/64"), 1, i0i1.GetInterfaceIndex (0));
  staticRoutingSrc->AddRoute (Ipv6Address ("2001:db8:0:5::/64"), 1, i0i1.GetInterfaceIndex (0));
  staticRoutingSrc->SetDefaultRoute (i0i1.GetAddress (1), 1);

  auto staticRoutingN0 = ipv6RoutingHelper.GetStaticRouting (n0->GetObject<Ipv6> ());
  staticRoutingN0->AddRoute (Ipv6Address ("2001:db8:0:1::/64"), 0, i1i2.GetInterfaceIndex (0));
  staticRoutingN0->AddRoute (Ipv6Address ("2001:db8:0:3::/64"), 1, i1i2.GetInterfaceIndex (1));
  staticRoutingN0->AddRoute (Ipv6Address ("2001:db8:0:4::/64"), 1, i1i2.GetInterfaceIndex (1));
  staticRoutingN0->AddRoute (Ipv6Address ("2001:db8:0:5::/64"), 1, i1i2.GetInterfaceIndex (1));

  auto staticRoutingR1 = ipv6RoutingHelper.GetStaticRouting (r1->GetObject<Ipv6> ());
  staticRoutingR1->AddRoute (Ipv6Address ("2001:db8:0:1::/64"), 0, i2i3.GetInterfaceIndex (0));
  staticRoutingR1->AddRoute (Ipv6Address ("2001:db8:0:2::/64"), 0, i2i3.GetInterfaceIndex (0));
  staticRoutingR1->AddRoute (Ipv6Address ("2001:db8:0:4::/64"), 1, i2i3.GetInterfaceIndex (1));
  staticRoutingR1->AddRoute (Ipv6Address ("2001:db8:0:5::/64"), 1, i2i3.GetInterfaceIndex (1));

  auto staticRoutingN1 = ipv6RoutingHelper.GetStaticRouting (n1->GetObject<Ipv6> ());
  staticRoutingN1->AddRoute (Ipv6Address ("2001:db8:0:1::/64"), 0, i3i4.GetInterfaceIndex (0));
  staticRoutingN1->AddRoute (Ipv6Address ("2001:db8:0:2::/64"), 0, i3i4.GetInterfaceIndex (0));
  staticRoutingN1->AddRoute (Ipv6Address ("2001:db8:0:3::/64"), 0, i3i4.GetInterfaceIndex (0));
  staticRoutingN1->AddRoute (Ipv6Address ("2001:db8:0:5::/64"), 1, i3i4.GetInterfaceIndex (1));

  auto staticRoutingR2 = ipv6RoutingHelper.GetStaticRouting (r2->GetObject<Ipv6> ());
  staticRoutingR2->AddRoute (Ipv6Address ("2001:db8:0:1::/64"), 0, i4i5.GetInterfaceIndex (0));
  staticRoutingR2->AddRoute (Ipv6Address ("2001:db8:0:2::/64"), 0, i4i5.GetInterfaceIndex (0));
  staticRoutingR2->AddRoute (Ipv6Address ("2001:db8:0:3::/64"), 0, i4i5.GetInterfaceIndex (0));
  staticRoutingR2->AddRoute (Ipv6Address ("2001:db8:0:4::/64"), 0, i4i5.GetInterfaceIndex (0));

  auto staticRoutingN2 = ipv6RoutingHelper.GetStaticRouting (n2->GetObject<Ipv6> ());
  staticRoutingN2->AddRoute (Ipv6Address ("2001:db8:0:1::/64"), 0, i4i5.GetInterfaceIndex (1));
  staticRoutingN2->AddRoute (Ipv6Address ("2001:db8:0:2::/64"), 0, i4i5.GetInterfaceIndex (1));
  staticRoutingN2->AddRoute (Ipv6Address ("2001:db8:0:3::/64"), 0, i4i5.GetInterfaceIndex (1));
  staticRoutingN2->AddRoute (Ipv6Address ("2001:db8:0:4::/64"), 0, i4i5.GetInterfaceIndex (1));

  NS_LOG_INFO ("Create Applications.");
  UdpEchoServerHelper echoServer (9);

  ApplicationContainer serverApps = echoServer.Install (n2);
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  UdpEchoClientHelper echoClient (i4i5.GetAddress (1), 9);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (1));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (4000));

  ApplicationContainer clientApps = echoClient.Install (src);
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  NS_LOG_INFO ("Enable Tracing.");
  if (tracing)
    {
      PointToPointHelper p2p;
      p2p.EnablePcapAll ("fragmentation-ipv6-PMTU");
    }

  Simulator::Run ();
  Simulator::Destroy ();
  NS_LOG_INFO ("Done.");

  return 0;
}