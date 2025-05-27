#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/ipv6-routing-table-entry.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/ipv6-extension-header.h"
#include "ns3/icmpv6-header.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("LooseSourceRoutingIpv6");

int main (int argc, char *argv[])
{
  bool verbose = false;
  bool tracing = true;

  CommandLine cmd;
  cmd.AddValue ("verbose", "Tell application to log if true.", verbose);
  cmd.AddValue ("tracing", "Enable pcap tracing.", tracing);

  cmd.Parse (argc, argv);

  if (verbose)
    {
      LogComponentEnable ("LooseSourceRoutingIpv6", LOG_LEVEL_INFO);
      LogComponentEnable ("Ipv6ExtensionHeader", LOG_LEVEL_ALL);
      LogComponentEnable ("Icmpv6L4Protocol", LOG_LEVEL_ALL);
      LogComponentEnable ("Ipv6StaticRouting", LOG_LEVEL_ALL);
    }

  NS_LOG_INFO ("Create nodes.");
  NodeContainer nodes;
  nodes.Create (6);
  Node *h0 = nodes.Get (0);
  Node *h1 = nodes.Get (1);
  Node *r0 = nodes.Get (2);
  Node *r1 = nodes.Get (3);
  Node *r2 = nodes.Get (4);
  Node *r3 = nodes.Get (5);

  NS_LOG_INFO ("Create channels.");
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer devh0r0 = p2p.Install (h0, r0);
  NetDeviceContainer devh1r3 = p2p.Install (h1, r3);
  NetDeviceContainer devr0r1 = p2p.Install (r0, r1);
  NetDeviceContainer devr0r2 = p2p.Install (r0, r2);
  NetDeviceContainer devr1r3 = p2p.Install (r1, r3);
  NetDeviceContainer devr2r3 = p2p.Install (r2, r3);

  NS_LOG_INFO ("Install Internet Stack.");
  InternetStackHelper internetv6;
  internetv6.Install (nodes);

  NS_LOG_INFO ("Assign IPv6 Addresses.");
  Ipv6AddressHelper ipv6;
  ipv6.SetBase (Ipv6Address ("2001:db8::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer ifh0r0 = ipv6.Assign (devh0r0);
  Ipv6InterfaceContainer ifr0r1 = ipv6.Assign (devr0r1);
  Ipv6InterfaceContainer ifr0r2 = ipv6.Assign (devr0r2);
  ipv6.SetBase (Ipv6Address ("2001:db8:1::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer ifr1r3 = ipv6.Assign (devr1r3);
  Ipv6InterfaceContainer ifr2r3 = ipv6.Assign (devr2r3);
  ipv6.SetBase (Ipv6Address ("2001:db8:2::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer ifh1r3 = ipv6.Assign (devh1r3);

  // Set IPv6 global addresses
  ifh0r0.GetAddress (0, 1).SetAsGlobal ();
  ifr0r1.GetAddress (0, 1).SetAsGlobal ();
  ifr0r2.GetAddress (0, 1).SetAsGlobal ();
  ifr1r3.GetAddress (0, 1).SetAsGlobal ();
  ifr2r3.GetAddress (0, 1).SetAsGlobal ();
  ifh1r3.GetAddress (0, 1).SetAsGlobal ();

  NS_LOG_INFO ("Create static routes.");
  Ptr<Ipv6StaticRouting> staticRouting;
  staticRouting = Ipv6RoutingHelper::GetRouting <Ipv6StaticRouting> (r0->GetObject<Ipv6> ()->GetRoutingProtocol (Ipv6::ROUTING_PROTOCOL_TYPE_STATIC));
  staticRouting->SetDefaultRoute (ifr0r1.GetAddress (1, 1), 0);

  staticRouting = Ipv6RoutingHelper::GetRouting <Ipv6StaticRouting> (r1->GetObject<Ipv6> ()->GetRoutingProtocol (Ipv6::ROUTING_PROTOCOL_TYPE_STATIC));
  staticRouting->SetDefaultRoute (ifr1r3.GetAddress (1, 1), 0);

  staticRouting = Ipv6RoutingHelper::GetRouting <Ipv6StaticRouting> (r2->GetObject<Ipv6> ()->GetRoutingProtocol (Ipv6::ROUTING_PROTOCOL_TYPE_STATIC));
  staticRouting->SetDefaultRoute (ifr2r3.GetAddress (1, 1), 0);

  staticRouting = Ipv6RoutingHelper::GetRouting <Ipv6StaticRouting> (r3->GetObject<Ipv6> ()->GetRoutingProtocol (Ipv6::ROUTING_PROTOCOL_TYPE_STATIC));
  staticRouting->SetDefaultRoute (ifh1r3.GetAddress (1, 1), 0);

  Ipv6StaticRoutingHelper helper;
  helper.AddRoute (h0, Ipv6Address ("2001:db8:2::1"), 0, ifh0r0.GetAddress (1, 1));

  // Populate Neighbor Cache
  Ipv6Address h1Addr = ifh1r3.GetAddress (0, 1);
  Ipv6Address r0Addr = ifh0r0.GetAddress (1, 1);
  Ipv6Address r1Addr = ifr1r3.GetAddress (1, 1);
  Ipv6Address r2Addr = ifr2r3.GetAddress (1, 1);

  // Loose Source Routing using IPv6 Extension Headers
  TypeId tid = TypeId::LookupByName ("ns3::Icmpv6L4Protocol");
  Ptr<Icmpv6L4Protocol> icmp = h0->GetObject<Ipv6> ()->GetProtocol (tid);

  icmp->SendIcmpv6EchoRequest (h1Addr, r0Addr, r1Addr, r2Addr);

  // Enable Tracing
  if (tracing)
    {
      p2p.EnablePcapAll ("loose-routing-ipv6");
    }

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}