#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/ipv6-routing-table-entry.h"
#include "ns3/icmpv6-header.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("Icmpv6Redirect");

int main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);
  LogComponent::SetDefaultLogLevel(LOG_LEVEL_ALL);
  LogComponent::SetDefaultLogComponentEnable(true);

  NodeContainer staNodes;
  staNodes.Create (2);

  NodeContainer routerNodes;
  routerNodes.Create (2);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer staRouterDevs1 = p2p.Install (staNodes.Get (0), routerNodes.Get (0));
  NetDeviceContainer staRouterDevs2 = p2p.Install (staNodes.Get (1), routerNodes.Get (1));
  NetDeviceContainer routerRouterDevs = p2p.Install (routerNodes.Get (0), routerNodes.Get (1));

  InternetStackHelper stack;
  stack.Install (staNodes);
  stack.Install (routerNodes);

  Ipv6AddressHelper ipv6;
  ipv6.SetBase (Ipv6Address ("2001:db8:1::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer staRouterIfaces1 = ipv6.Assign (staRouterDevs1);

  ipv6.SetBase (Ipv6Address ("2001:db8:2::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer staRouterIfaces2 = ipv6.Assign (staRouterDevs2);

  ipv6.SetBase (Ipv6Address ("2001:db8:3::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer routerRouterIfaces = ipv6.Assign (routerRouterDevs);

  Ipv6StaticRoutingHelper ipv6RoutingHelper;

  Ptr<Ipv6StaticRouting> sta1Routing = ipv6RoutingHelper.GetOrCreateRouting (staNodes.Get (0)->GetObject<Ipv6> ());
  sta1Routing->SetDefaultRoute (staRouterIfaces1.GetAddress (1, 0), 0);

  Ptr<Ipv6StaticRouting> sta2Routing = ipv6RoutingHelper.GetOrCreateRouting (staNodes.Get (1)->GetObject<Ipv6> ());
  sta2Routing->SetDefaultRoute (staRouterIfaces2.GetAddress (1, 0), 0);

  Ptr<Ipv6StaticRouting> router1Routing = ipv6RoutingHelper.GetOrCreateRouting (routerNodes.Get (0)->GetObject<Ipv6> ());
  router1Routing->AddRouteToNetwork (Ipv6Address ("2001:db8:2::"), Ipv6Prefix (64), routerRouterIfaces.GetAddress (1, 0), 1);

  Ipv6Address sinkAddr = staRouterIfaces2.GetAddress (0, 0);

  V6PingHelper ping6 (sinkAddr);
  ping6.SetAttribute ("Verbose", BooleanValue (true));
  ApplicationContainer apps = ping6.Install (staNodes.Get (0));
  apps.Start (Seconds (1.0));
  apps.Stop (Seconds (10.0));

  p2p.EnablePcapAll("icmpv6-redirect", false);

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}