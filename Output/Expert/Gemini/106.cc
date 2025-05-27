#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/ipv6-routing-table-entry.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("FragmentationIpv6PMTU");

int main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);
  LogComponent::SetAttribute ("UdpEchoClientApplication", "MaxPackets", UintegerValue (1));

  NodeContainer srcNode, n0Node, r1Node, n1Node, r2Node, n2Node;
  srcNode.Create (1);
  n0Node.Create (1);
  r1Node.Create (1);
  n1Node.Create (1);
  r2Node.Create (1);
  n2Node.Create (1);

  PointToPointHelper p2pSrcR1, p2pR1N1, p2pN1R2, p2pR2N2;
  p2pSrcR1.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  p2pSrcR1.SetChannelAttribute ("Delay", StringValue ("2ms"));
  p2pSrcR1.SetDeviceAttribute ("Mtu", UintegerValue (5000));

  p2pR1N1.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  p2pR1N1.SetChannelAttribute ("Delay", StringValue ("2ms"));
  p2pR1N1.SetDeviceAttribute ("Mtu", UintegerValue (2000));

  p2pN1R2.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  p2pN1R2.SetChannelAttribute ("Delay", StringValue ("2ms"));
  p2pN1R2.SetDeviceAttribute ("Mtu", UintegerValue (1500));

  p2pR2N2.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  p2pR2N2.SetChannelAttribute ("Delay", StringValue ("2ms"));
  p2pR2N2.SetDeviceAttribute ("Mtu", UintegerValue (1500));

  NetDeviceContainer devSrcR1 = p2pSrcR1.Install (srcNode.Get (0), r1Node.Get (0));
  NetDeviceContainer devR1N1 = p2pR1N1.Install (r1Node.Get (0), n1Node.Get (0));
  NetDeviceContainer devN1R2 = p2pN1R2.Install (n1Node.Get (0), r2Node.Get (0));
  NetDeviceContainer devR2N2 = p2pR2N2.Install (r2Node.Get (0), n2Node.Get (0));

  CsmaHelper csmaN0R1, csmaN1R2;
  csmaN0R1.SetChannelAttribute ("DataRate", StringValue ("10Mbps"));
  csmaN0R1.SetChannelAttribute ("Delay", StringValue ("2ms"));
  csmaN1R2.SetChannelAttribute ("DataRate", StringValue ("10Mbps"));
  csmaN1R2.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer devN0R1 = csmaN0R1.Install (NodeContainer (n0Node.Get (0), r1Node.Get (0)));
  NetDeviceContainer devN1R2_ = csmaN1R2.Install (NodeContainer (n1Node.Get (0), r2Node.Get (0)));

  InternetStackHelper internetv6;
  internetv6.Install (NodeContainer (srcNode, n0Node, r1Node, n1Node, r2Node, n2Node));

  Ipv6AddressHelper ipv6;
  ipv6.SetBase (Ipv6Address ("2001:1::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer iSrcR1 = ipv6.Assign (devSrcR1);

  ipv6.SetBase (Ipv6Address ("2001:2::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer iN0R1 = ipv6.Assign (devN0R1);

  ipv6.SetBase (Ipv6Address ("2001:3::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer iR1N1 = ipv6.Assign (devR1N1);

  ipv6.SetBase (Ipv6Address ("2001:4::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer iN1R2 = ipv6.Assign (devN1R2_);

  ipv6.SetBase (Ipv6Address ("2001:5::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer iR2N2 = ipv6.Assign (devR2N2);

  Ipv6StaticRoutingHelper ipv6RoutingHelper;

  auto staticRouting = ipv6RoutingHelper.GetStaticRouting (srcNode.Get (0)->GetObject<Ipv6> ());
  staticRouting->AddGlobalRoute (Ipv6Address ("2001:2::/64"), iSrcR1.GetAddress (1, 0), 1);
  staticRouting->AddGlobalRoute (Ipv6Address ("2001:3::/64"), iSrcR1.GetAddress (1, 0), 1);
  staticRouting->AddGlobalRoute (Ipv6Address ("2001:4::/64"), iSrcR1.GetAddress (1, 0), 1);
  staticRouting->AddGlobalRoute (Ipv6Address ("2001:5::/64"), iSrcR1.GetAddress (1, 0), 1);

  staticRouting = ipv6RoutingHelper.GetStaticRouting (n0Node.Get (0)->GetObject<Ipv6> ());
  staticRouting->AddGlobalRoute (Ipv6Address ("2001:1::/64"), iN0R1.GetAddress (1, 0), 1);
  staticRouting->AddGlobalRoute (Ipv6Address ("2001:3::/64"), iN0R1.GetAddress (1, 0), 1);
  staticRouting->AddGlobalRoute (Ipv6Address ("2001:4::/64"), iN0R1.GetAddress (1, 0), 1);
  staticRouting->AddGlobalRoute (Ipv6Address ("2001:5::/64"), iN0R1.GetAddress (1, 0), 1);

  staticRouting = ipv6RoutingHelper.GetStaticRouting (r1Node.Get (0)->GetObject<Ipv6> ());
  staticRouting->AddGlobalRoute (Ipv6Address ("2001:1::/64"), iSrcR1.GetAddress (0, 0), 1);
  staticRouting->AddGlobalRoute (Ipv6Address ("2001:5::/64"), iR1N1.GetAddress (1, 0), 1);

  staticRouting = ipv6RoutingHelper.GetStaticRouting (n1Node.Get (0)->GetObject<Ipv6> ());
  staticRouting->AddGlobalRoute (Ipv6Address ("2001:1::/64"), iR1N1.GetAddress (0, 0), 1);
  staticRouting->AddGlobalRoute (Ipv6Address ("2001:2::/64"), iR1N1.GetAddress (0, 0), 1);
  staticRouting->AddGlobalRoute (Ipv6Address ("2001:5::/64"), iN1R2.GetAddress (1, 0), 1);

  staticRouting = ipv6RoutingHelper.GetStaticRouting (r2Node.Get (0)->GetObject<Ipv6> ());
  staticRouting->AddGlobalRoute (Ipv6Address ("2001:1::/64"), iN1R2.GetAddress (0, 0), 1);
  staticRouting->AddGlobalRoute (Ipv6Address ("2001:2::/64"), iN1R2.GetAddress (0, 0), 1);
  staticRouting->AddGlobalRoute (Ipv6Address ("2001:3::/64"), iR2N2.GetAddress (1, 0), 1);

  staticRouting = ipv6RoutingHelper.GetStaticRouting (n2Node.Get (0)->GetObject<Ipv6> ());
  staticRouting->AddGlobalRoute (Ipv6Address ("2001:1::/64"), iR2N2.GetAddress (0, 0), 1);
  staticRouting->AddGlobalRoute (Ipv6Address ("2001:2::/64"), iR2N2.GetAddress (0, 0), 1);
  staticRouting->AddGlobalRoute (Ipv6Address ("2001:3::/64"), iR2N2.GetAddress (0, 0), 1);
  staticRouting->AddGlobalRoute (Ipv6Address ("2001:4::/64"), iR2N2.GetAddress (0, 0), 1);

  UdpEchoServerHelper echoServer (9);
  ApplicationContainer serverApps = echoServer.Install (n2Node.Get (0));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  UdpEchoClientHelper echoClient (iR2N2.GetAddress (1, 0), 9);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (1));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (4000));
  ApplicationContainer clientApps = echoClient.Install (srcNode.Get (0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  Packet::EnableAsciiAll ("fragmentation-ipv6-PMTU.tr");

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}