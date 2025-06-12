#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv6-static-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("Ipv6FragmentationSimulation");

int main (int argc, char *argv[])
{
  LogComponent::SetAttribute ("UdpEchoClientApplication", "PacketSize", UintegerValue (4000));
  CommandLine cmd;
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);
  LogComponent::SetLevel (LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create (6);
  NodeContainer src = NodeContainer (nodes.Get (0));
  NodeContainer n0 = NodeContainer (nodes.Get (1));
  NodeContainer r1 = NodeContainer (nodes.Get (2));
  NodeContainer n1 = NodeContainer (nodes.Get (3));
  NodeContainer r2 = NodeContainer (nodes.Get (4));
  NodeContainer n2 = NodeContainer (nodes.Get (5));

  PointToPointHelper p2p_sr1;
  p2p_sr1.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  p2p_sr1.SetChannelAttribute ("Delay", StringValue ("2ms"));
  p2p_sr1.SetDeviceAttribute ("Mtu", UintegerValue (5000));
  NetDeviceContainer dev_sr1 = p2p_sr1.Install (src, r1);

  CsmaHelper csma_r1n0;
  csma_r1n0.SetChannelAttribute ("DataRate", StringValue ("10Mbps"));
  csma_r1n0.SetChannelAttribute ("Delay", TimeValue (NanoSeconds (100)));
  csma_r1n0.SetDeviceAttribute ("Mtu", UintegerValue (2000));
  NetDeviceContainer dev_r1n0 = csma_r1n0.Install (NodeContainer (r1, n0));

  PointToPointHelper p2p_r1r2;
  p2p_r1r2.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  p2p_r1r2.SetChannelAttribute ("Delay", StringValue ("2ms"));
  p2p_r1r2.SetDeviceAttribute ("Mtu", UintegerValue (2000));
  NetDeviceContainer dev_r1r2 = p2p_r1r2.Install (r1, r2);

  CsmaHelper csma_r2n1;
  csma_r2n1.SetChannelAttribute ("DataRate", StringValue ("10Mbps"));
  csma_r2n1.SetChannelAttribute ("Delay", TimeValue (NanoSeconds (100)));
  csma_r2n1.SetDeviceAttribute ("Mtu", UintegerValue (1500));
  NetDeviceContainer dev_r2n1 = csma_r2n1.Install (NodeContainer (r2, n1));

  PointToPointHelper p2p_r2n2;
  p2p_r2n2.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  p2p_r2n2.SetChannelAttribute ("Delay", StringValue ("2ms"));
  p2p_r2n2.SetDeviceAttribute ("Mtu", UintegerValue (1500));
  NetDeviceContainer dev_r2n2 = p2p_r2n2.Install (r2, n2);

  InternetStackHelper internetv6;
  internetv6.Install (nodes);

  Ipv6AddressHelper ipv6;
  ipv6.SetBase (Ipv6Address ("2001:1::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer if_sr1 = ipv6.Assign (dev_sr1);

  ipv6.SetBase (Ipv6Address ("2001:2::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer if_r1n0 = ipv6.Assign (dev_r1n0);

  ipv6.SetBase (Ipv6Address ("2001:3::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer if_r1r2 = ipv6.Assign (dev_r1r2);

  ipv6.SetBase (Ipv6Address ("2001:4::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer if_r2n1 = ipv6.Assign (dev_r2n1);

  ipv6.SetBase (Ipv6Address ("2001:5::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer if_r2n2 = ipv6.Assign (dev_r2n2);

  Ipv6StaticRoutingHelper ipv6RoutingHelper;
  Ptr<Ipv6StaticRouting> staticRoutingSrc = ipv6RoutingHelper.GetStaticRouting (src.Get (0)->GetObject<Ipv6> ());
  staticRoutingSrc->AddGlobalRoute (Ipv6Address ("2001:2::/64"), if_sr1.GetAddress (1), 1);
  staticRoutingSrc->AddGlobalRoute (Ipv6Address ("2001:3::/64"), if_sr1.GetAddress (1), 1);
  staticRoutingSrc->AddGlobalRoute (Ipv6Address ("2001:4::/64"), if_sr1.GetAddress (1), 1);
  staticRoutingSrc->AddGlobalRoute (Ipv6Address ("2001:5::/64"), if_sr1.GetAddress (1), 1);

  Ptr<Ipv6StaticRouting> staticRoutingR1 = ipv6RoutingHelper.GetStaticRouting (r1.Get (0)->GetObject<Ipv6> ());
  staticRoutingR1->AddGlobalRoute (Ipv6Address ("2001:1::/64"), if_sr1.GetAddress (0), 1);
  staticRoutingR1->AddGlobalRoute (Ipv6Address ("2001:4::/64"), if_r1r2.GetAddress (1), 1);
  staticRoutingR1->AddGlobalRoute (Ipv6Address ("2001:5::/64"), if_r1r2.GetAddress (1), 1);

  Ptr<Ipv6StaticRouting> staticRoutingR2 = ipv6RoutingHelper.GetStaticRouting (r2.Get (0)->GetObject<Ipv6> ());
  staticRoutingR2->AddGlobalRoute (Ipv6Address ("2001:1::/64"), if_r1r2.GetAddress (0), 1);
  staticRoutingR2->AddGlobalRoute (Ipv6Address ("2001:2::/64"), if_r1r2.GetAddress (0), 1);

  Ipv6Address srcAddr = if_sr1.GetAddress (0);
  Ipv6Address n0Addr = if_r1n0.GetAddress (1);
  Ipv6Address n1Addr = if_r2n1.GetAddress (1);
  Ipv6Address n2Addr = if_r2n2.GetAddress (1);

  UdpEchoServerHelper echoServer (9);
  ApplicationContainer serverApps = echoServer.Install (NodeContainer (n0.Get(0), n1.Get(0), n2.Get(0)));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  UdpEchoClientHelper echoClient (n2Addr, 9);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (5));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (4000));
  ApplicationContainer clientApps = echoClient.Install (src.Get (0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  echoClient.SetAttribute ("RemoteAddress", AddressValue(n1Addr));
  clientApps = echoClient.Install (src.Get (0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  echoClient.SetAttribute ("RemoteAddress", AddressValue(n0Addr));
  clientApps = echoClient.Install (src.Get (0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  Simulator::Stop (Seconds (11.0));

  PointToPointHelper::EnablePcapAll ("fragmentation-ipv6-PMTU");
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}