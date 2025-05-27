#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/ipv6-list-routing.h"
#include "ns3/icmpv6-l4-protocol.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/ipv6-extension-header.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("LooseSourceRoutingIpv6");

int main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);
  LogComponent::SetDefaultLogLevel (LOG_LEVEL_INFO);
  LogComponent::SetDefaultLogComponentEnable (true);

  NodeContainer nodes;
  nodes.Create (6);

  NodeContainer host0 = NodeContainer (nodes.Get (0));
  NodeContainer host1 = NodeContainer (nodes.Get (1));
  NodeContainer router0 = NodeContainer (nodes.Get (2));
  NodeContainer router1 = NodeContainer (nodes.Get (3));
  NodeContainer router2 = NodeContainer (nodes.Get (4));
  NodeContainer router3 = NodeContainer (nodes.Get (5));

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer devices02 = p2p.Install (host0, router0);
  NetDeviceContainer devices13 = p2p.Install (host1, router3);
  NetDeviceContainer devices21 = p2p.Install (router0, router1);
  NetDeviceContainer devices23 = p2p.Install (router0, router2);
  NetDeviceContainer devices31 = p2p.Install (router3, router1);
  NetDeviceContainer devices32 = p2p.Install (router3, router2);
  NetDeviceContainer devices12 = p2p.Install (router1, router2);

  InternetStackHelper internetv6;
  internetv6.Install (nodes);

  Ipv6AddressHelper ipv6;
  ipv6.SetBase (Ipv6Address ("2001:db8:0:1::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer interfaces02 = ipv6.Assign (devices02);

  ipv6.SetBase (Ipv6Address ("2001:db8:0:2::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer interfaces13 = ipv6.Assign (devices13);

  ipv6.SetBase (Ipv6Address ("2001:db8:0:3::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer interfaces21 = ipv6.Assign (devices21);

  ipv6.SetBase (Ipv6Address ("2001:db8:0:4::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer interfaces23 = ipv6.Assign (devices23);

  ipv6.SetBase (Ipv6Address ("2001:db8:0:5::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer interfaces31 = ipv6.Assign (devices31);

  ipv6.SetBase (Ipv6Address ("2001:db8:0:6::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer interfaces32 = ipv6.Assign (devices32);

  ipv6.SetBase (Ipv6Address ("2001:db8:0:7::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer interfaces12 = ipv6.Assign (devices12);

  Ipv6StaticRoutingHelper ipv6RoutingHelper;
  Ptr<Ipv6StaticRouting> staticRouting0 = ipv6RoutingHelper.GetStaticRouting (router0.Get (0)->GetObject<Ipv6> ());
  staticRouting0->AddHostRouteToNetmask (Ipv6Address ("2001:db8:0:2::1"), 64, 1);
  staticRouting0->AddHostRouteToNetmask (Ipv6Address ("2001:db8:0:5::1"), 64, 2);

  Ptr<Ipv6StaticRouting> staticRouting1 = ipv6RoutingHelper.GetStaticRouting (router1.Get (0)->GetObject<Ipv6> ());
  staticRouting1->AddHostRouteToNetmask (Ipv6Address ("2001:db8:0:1::1"), 64, 0);
  staticRouting1->AddHostRouteToNetmask (Ipv6Address ("2001:db8:0:6::1"), 64, 2);

  Ptr<Ipv6StaticRouting> staticRouting2 = ipv6RoutingHelper.GetStaticRouting (router2.Get (0)->GetObject<Ipv6> ());
  staticRouting2->AddHostRouteToNetmask (Ipv6Address ("2001:db8:0:1::1"), 64, 0);
  staticRouting2->AddHostRouteToNetmask (Ipv6Address ("2001:db8:0:5::1"), 64, 1);

  Ptr<Ipv6StaticRouting> staticRouting3 = ipv6RoutingHelper.GetStaticRouting (router3.Get (0)->GetObject<Ipv6> ());
  staticRouting3->AddHostRouteToNetmask (Ipv6Address ("2001:db8:0:2::1"), 64, 0);
  staticRouting3->AddHostRouteToNetmask (Ipv6Address ("2001:db8:0:7::1"), 64, 1);

  for (int i = 2; i < 6; ++i)
    {
      Ptr<Node> router = nodes.Get (i);
      Ipv6ListRoutingHelper listRouting;
      Ipv6StaticRoutingHelper staticRoutingHelper;

      listRouting.Add (staticRoutingHelper, 0);
      internetv6.SetRoutingHelper (listRouting);

      Ipv6Address addr1 ("2001:db8:0:1::1");
      Ipv6Address addr2 ("2001:db8:0:2::1");
      Ipv6Address addr3 ("2001:db8:0:3::1");
      Ipv6Address addr4 ("2001:db8:0:4::1");
      Ipv6Address addr5 ("2001:db8:0:5::1");
      Ipv6Address addr6 ("2001:db8:0:6::1");
      Ipv6Address addr7 ("2001:db8:0:7::1");

      if (router == router0.Get (0)) {
            staticRoutingHelper.PopulateRoutingTableAssumingDirectConnectivity(router);
        }
      if (router == router1.Get (0)) {
            staticRoutingHelper.PopulateRoutingTableAssumingDirectConnectivity(router);
        }
        if (router == router2.Get (0)) {
            staticRoutingHelper.PopulateRoutingTableAssumingDirectConnectivity(router);
        }
        if (router == router3.Get (0)) {
            staticRoutingHelper.PopulateRoutingTableAssumingDirectConnectivity(router);
        }
    }

  TypeId tid = TypeId::LookupByName ("ns3::Icmpv6L4Protocol");
  Ptr<Node> n0 = nodes.Get (0);
  Ptr<Icmpv6L4Protocol> icmpv6 = n0->GetObject<Icmpv6L4Protocol> (tid);
  icmpv6->SetForwarding (true);

  TypeId echoClientTid = TypeId::LookupByName ("ns3::Icmpv6EchoClient");
  Ptr<Icmpv6EchoClient> echoClient = DynamicCast<Icmpv6EchoClient> (CreateObject (echoClientTid));
  echoClient->SetRemote (interfaces13.GetAddress (0));
  echoClient->SetIfIndex (1);
  echoClient->SetHopLimit (255);
  ApplicationContainer echoClientApp;
  echoClientApp.Add (echoClient);
  host0.Get (0)->AddApplication (echoClient);

  Ipv6Address addr1 ("2001:db8:0:3::2");
  Ipv6Address addr2 ("2001:db8:0:6::2");
  std::vector<Ipv6Address> route;
    route.push_back(addr1);
    route.push_back(addr2);

  echoClient->SetLooseRouting (route);
  echoClientApp.Start (Seconds (1.0));
  echoClientApp.Stop (Seconds (10.0));

  Simulator::Stop (Seconds (11.0));

  p2p.EnablePcapAll ("loose-routing-ipv6");

  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}