#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/ipv6-routing-table-entry.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("FragmentationIPv6PMTU");

int main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);
  LogComponent::EnableIlog (LOG_LEVEL_ALL);
  LogComponent::SetOutputFile ("fragmentation-ipv6-PMTU.log");
  LogComponent::Enable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
  LogComponent::Enable ("UdpEchoServerApplication", LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create (6);

  NodeContainer n0r1 = NodeContainer (nodes.Get (0), nodes.Get (1), nodes.Get (2));
  NodeContainer n1r1 = NodeContainer (nodes.Get (3), nodes.Get (1));
  NodeContainer n2r2 = NodeContainer (nodes.Get (4), nodes.Get (2));
  NodeContainer n1r2 = NodeContainer (nodes.Get (3), nodes.Get (2), nodes.Get (4));
  NodeContainer srcn0 = NodeContainer (nodes.Get (5), nodes.Get (0));

  CsmaHelper csma_srcn0;
  csma_srcn0.SetChannelAttribute ("DataRate", DataRateValue (DataRate ("100Mbps")));
  csma_srcn0.SetChannelAttribute ("Delay", TimeValue (NanoSeconds (6560)));
  csma_srcn0.SetDeviceAttribute ("Mtu", UintegerValue (5000));
  NetDeviceContainer d_srcn0 = csma_srcn0.Install (srcn0);

  PointToPointHelper p2p_n0r1;
  p2p_n0r1.SetDeviceAttribute ("DataRate", StringValue ("100Mbps"));
  p2p_n0r1.SetChannelAttribute ("Delay", StringValue ("20ms"));
  p2p_n0r1.SetDeviceAttribute ("Mtu", UintegerValue (2000));
  NetDeviceContainer d_n0r1 = p2p_n0r1.Install (n0r1);

  PointToPointHelper p2p_n1r1;
  p2p_n1r1.SetDeviceAttribute ("DataRate", StringValue ("100Mbps"));
  p2p_n1r1.SetChannelAttribute ("Delay", StringValue ("20ms"));
  p2p_n1r1.SetDeviceAttribute ("Mtu", UintegerValue (1500));
  NetDeviceContainer d_n1r1 = p2p_n1r1.Install (n1r1);

  PointToPointHelper p2p_n2r2;
  p2p_n2r2.SetDeviceAttribute ("DataRate", StringValue ("100Mbps"));
  p2p_n2r2.SetChannelAttribute ("Delay", StringValue ("20ms"));
  p2p_n2r2.SetDeviceAttribute ("Mtu", UintegerValue (1500));
  NetDeviceContainer d_n2r2 = p2p_n2r2.Install (n2r2);

  PointToPointHelper p2p_n1r2;
  p2p_n1r2.SetDeviceAttribute ("DataRate", StringValue ("100Mbps"));
  p2p_n1r2.SetChannelAttribute ("Delay", StringValue ("20ms"));
  p2p_n1r2.SetDeviceAttribute ("Mtu", UintegerValue (1500));
  NetDeviceContainer d_n1r2 = p2p_n1r2.Install (n1r2);

  InternetStackHelper internetv6;
  internetv6.Install (nodes);

  Ipv6AddressHelper ipv6;
  ipv6.SetBase (Ipv6Address ("2001:db8:0:1::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer i_srcn0 = ipv6.Assign (d_srcn0);
  ipv6.SetBase (Ipv6Address ("2001:db8:0:2::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer i_n0r1 = ipv6.Assign (d_n0r1);
  ipv6.SetBase (Ipv6Address ("2001:db8:0:3::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer i_n1r1 = ipv6.Assign (d_n1r1);
  ipv6.SetBase (Ipv6Address ("2001:db8:0:4::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer i_n2r2 = ipv6.Assign (d_n2r2);
  ipv6.SetBase (Ipv6Address ("2001:db8:0:5::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer i_n1r2 = ipv6.Assign (d_n1r2);

  Ipv6StaticRoutingHelper ipv6RoutingHelper;

  auto router0RoutingTable = ipv6RoutingHelper.GetIpv6StaticRouting (nodes.Get (1)->GetObject<Ipv6> ());
  router0RoutingTable->AddRouteToNet (Ipv6Address ("2001:db8:0:3::"), Ipv6Prefix (64), i_n1r1.GetAddress (1), 1);
  router0RoutingTable->AddRouteToNet (Ipv6Address ("2001:db8:0:4::"), Ipv6Prefix (64), i_n0r1.GetAddress (2), 1);

  auto router1RoutingTable = ipv6RoutingHelper.GetIpv6StaticRouting (nodes.Get (2)->GetObject<Ipv6> ());
  router1RoutingTable->AddRouteToNet (Ipv6Address ("2001:db8:0:2::"), Ipv6Prefix (64), i_n0r1.GetAddress (1), 1);
  router1RoutingTable->AddRouteToNet (Ipv6Address ("2001:db8:0:3::"), Ipv6Prefix (64), i_n1r2.GetAddress (1), 1);

  Ipv6StaticRoutingHelper::PopulateRoutingTables ();

  UdpEchoServerHelper echoServer (9);
  ApplicationContainer serverApps = echoServer.Install (nodes.Get (4));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  UdpEchoClientHelper echoClient (i_n2r2.GetAddress (0), 9);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (10));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (4000));
  ApplicationContainer clientApps = echoClient.Install (nodes.Get (5));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  nodes.Get(5)->GetObject<Ipv6>()->SetDefaultRoute (i_srcn0.GetAddress(1));
  nodes.Get(3)->GetObject<Ipv6>()->SetDefaultRoute (i_n1r1.GetAddress(1));

  Simulator::Stop (Seconds (11.0));

  AsciiTraceHelper ascii;
  Ptr<OutputStreamWrapper> stream = ascii.CreateFileStream ("fragmentation-ipv6-PMTU.tr");
  p2p_n0r1.EnableAsciiAll (stream);

  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}