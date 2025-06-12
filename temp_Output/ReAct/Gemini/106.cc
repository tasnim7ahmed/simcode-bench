#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/ipv6-routing-table-entry.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("Ipv6Fragmentation");

int main (int argc, char *argv[])
{
  bool verbose = false;
  bool tracing = true;

  CommandLine cmd (__FILE__);
  cmd.AddValue ("verbose", "Tell application to log if true", verbose);
  cmd.AddValue ("tracing", "Enable pcap tracing", tracing);

  cmd.Parse (argc, argv);

  if (verbose)
    {
      LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
      LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);
    }

  NodeContainer p2pNodes;
  p2pNodes.Create (6);

  NodeContainer n0n1 = NodeContainer (p2pNodes.Get (1), p2pNodes.Get (3));
  NodeContainer r1r2 = NodeContainer (p2pNodes.Get (2), p2pNodes.Get (4));

  CsmaHelper csma5000;
  csma5000.SetChannelAttribute ("DataRate", StringValue ("10Mbps"));
  csma5000.SetChannelAttribute ("Delay", TimeValue (NanoSeconds (100)));
  csma5000.SetDeviceAttribute ("Mtu", UintegerValue (5000));

  CsmaHelper csma2000;
  csma2000.SetChannelAttribute ("DataRate", StringValue ("10Mbps"));
  csma2000.SetChannelAttribute ("Delay", TimeValue (NanoSeconds (100)));
  csma2000.SetDeviceAttribute ("Mtu", UintegerValue (2000));

  CsmaHelper csma1500;
  csma1500.SetChannelAttribute ("DataRate", StringValue ("10Mbps"));
  csma1500.SetChannelAttribute ("Delay", TimeValue (NanoSeconds (100)));
  csma1500.SetDeviceAttribute ("Mtu", UintegerValue (1500));

  NetDeviceContainer devices5000_0 = csma5000.Install (NodeContainer (p2pNodes.Get (0), p2pNodes.Get (2)));
  NetDeviceContainer devices5000_1 = csma5000.Install (NodeContainer (p2pNodes.Get (1), p2pNodes.Get (2)));

  NetDeviceContainer devices2000 = csma2000.Install (n0n1);
  NetDeviceContainer devices1500 = csma1500.Install (r1r2);

  InternetStackHelper internetv6;
  internetv6.Install (p2pNodes);

  Ipv6AddressHelper ipv6;
  ipv6.SetBase (Ipv6Address ("2001:db8:1::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer interfaces5000_0 = ipv6.Assign (devices5000_0);
  Ipv6InterfaceContainer interfaces5000_1 = ipv6.Assign (devices5000_1);

  ipv6.SetBase (Ipv6Address ("2001:db8:2::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer interfaces2000 = ipv6.Assign (devices2000);

  ipv6.SetBase (Ipv6Address ("2001:db8:3::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer interfaces1500 = ipv6.Assign (devices1500);

  for (uint32_t i = 0; i < p2pNodes.GetN (); ++i)
  {
    Ptr<Ipv6> ipv6_ = p2pNodes.Get (i)->GetObject<Ipv6> ();
    ipv6_->SetForwarding (i, true);
  }

  Ipv6StaticRoutingHelper ipv6RoutingHelper;
  Ptr<Ipv6StaticRouting> staticRouting;

  staticRouting = ipv6RoutingHelper.GetOrCreateRouting (p2pNodes.Get (0)->GetObject<Ipv6> (0));
  staticRouting->AddHostRouteTo (Ipv6Address ("2001:db8:2::2"), 0, 1);

  staticRouting = ipv6RoutingHelper.GetOrCreateRouting (p2pNodes.Get (1)->GetObject<Ipv6> (0));
  staticRouting->AddHostRouteTo (Ipv6Address ("2001:db8:1::1"), 0, 2);

  staticRouting = ipv6RoutingHelper.GetOrCreateRouting (p2pNodes.Get (3)->GetObject<Ipv6> (0));
  staticRouting->AddHostRouteTo (Ipv6Address ("2001:db8:1::1"), 0, 2);

  staticRouting = ipv6RoutingHelper.GetOrCreateRouting (p2pNodes.Get (5)->GetObject<Ipv6> (0));
  staticRouting->AddHostRouteTo (Ipv6Address ("2001:db8:1::1"), 0, 1);
  staticRouting->AddHostRouteTo (Ipv6Address ("2001:db8:2::2"), 0, 1);

  UdpEchoServerHelper echoServer (9);
  ApplicationContainer serverApps = echoServer.Install (p2pNodes.Get (5));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  UdpEchoClientHelper echoClient (interfaces1500.GetAddress (1), 9);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (1));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (4000));
  ApplicationContainer clientApps = echoClient.Install (p2pNodes.Get (0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  if (tracing == true)
    {
      AsciiTraceHelper ascii;
      csma5000.EnableAsciiAll (ascii.CreateFileStream ("fragmentation-ipv6-PMTU.tr"));
    }

  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}