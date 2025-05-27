#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/ipv6-routing-table-entry.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("FragmentationIpv6TwoMtu");

int main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);
  LogComponent::SetAttribute ("UdpEchoClientApplication", "MaxPackets", UintegerValue (1));

  NodeContainer src_n0_r_n1_dst;
  src_n0_r_n1_dst.Create (5);
  Ptr<Node> Src = src_n0_r_n1_dst.Get (0);
  Ptr<Node> n0 = src_n0_r_n1_dst.Get (1);
  Ptr<Node> r = src_n0_r_n1_dst.Get (2);
  Ptr<Node> n1 = src_n0_r_n1_dst.Get (3);
  Ptr<Node> Dst = src_n0_r_n1_dst.Get (4);

  NodeContainer n0_r = NodeContainer (n0, r);
  NodeContainer r_n1 = NodeContainer (r, n1);

  CsmaHelper csmaN0R;
  csmaN0R.SetChannelAttribute ("DataRate", DataRateValue (1000000));
  csmaN0R.SetChannelAttribute ("Delay", TimeValue (MicroSeconds (2)));
  csmaN0R.SetDeviceAttribute ("Mtu", UintegerValue (5000));
  NetDeviceContainer ndN0R = csmaN0R.Install (n0_r);

  CsmaHelper csmaRN1;
  csmaRN1.SetChannelAttribute ("DataRate", DataRateValue (1000000));
  csmaRN1.SetChannelAttribute ("Delay", TimeValue (MicroSeconds (2)));
  csmaRN1.SetDeviceAttribute ("Mtu", UintegerValue (1500));
  NetDeviceContainer ndRN1 = csmaRN1.Install (r_n1);

  InternetStackHelper internetv6;
  internetv6.Install (src_n0_r_n1_dst);

  Ipv6AddressHelper ipv6;
  ipv6.SetBase (Ipv6Address ("2001:db8:1::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer iN0R = ipv6.Assign (ndN0R);

  ipv6.SetBase (Ipv6Address ("2001:db8:2::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer iRN1 = ipv6.Assign (ndRN1);

  Ipv6StaticRoutingHelper ipv6RoutingHelper;
  Ptr<Ipv6StaticRouting> staticRoutingR = ipv6RoutingHelper.GetStaticRouting (r->GetObject<Ipv6> ());
  staticRoutingR->AddHostRouteTo (Ipv6Address ("2001:db8:1::1"), 0, 1);
  staticRoutingR->AddHostRouteTo (Ipv6Address ("2001:db8:2::2"), 1, 1);

  Ptr<Ipv6StaticRouting> staticRoutingN0 = ipv6RoutingHelper.GetStaticRouting (n0->GetObject<Ipv6> ());
  staticRoutingN0->AddHostRouteTo (Ipv6Address ("2001:db8:2::2"), 0, 1);

  Ptr<Ipv6StaticRouting> staticRoutingN1 = ipv6RoutingHelper.GetStaticRouting (n1->GetObject<Ipv6> ());
  staticRoutingN1->AddHostRouteTo (Ipv6Address ("2001:db8:1::1"), 0, 1);

  UdpEchoServerHelper echoServer (9);
  ApplicationContainer serverApps = echoServer.Install (n1);
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  UdpEchoClientHelper echoClient (iRN1.GetAddress (1), 9);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (1));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (4000));
  ApplicationContainer clientApps = echoClient.Install (n0);
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  CsmaHelper::EnableAsciiAll ("fragmentation-ipv6-two-mtu.tr");

  Simulator::Stop (Seconds (11.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}