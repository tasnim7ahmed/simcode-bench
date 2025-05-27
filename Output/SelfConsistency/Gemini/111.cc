#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/ipv6-routing-table-entry.h"
#include "ns3/radvd-module.h"
#include "ns3/icmpv6-echo.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("RadvdExample");

int main (int argc, char *argv[])
{
  LogComponent::SetAttribute ("Icmpv6L4Protocol", "ChecksumEnabled", BooleanValue (true));

  CommandLine cmd;
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);

  NodeContainer nodes;
  nodes.Create (2);

  NodeContainer router;
  router.Create (1);

  CsmaHelper csma0;
  csma0.SetChannelAttribute ("DataRate", DataRateValue (1000000));
  csma0.SetChannelAttribute ("Delay", TimeValue (MicroSeconds (2)));
  NetDeviceContainer devices0 = csma0.Install (NodeContainer (nodes.Get (0), router.Get (0)));

  CsmaHelper csma1;
  csma1.SetChannelAttribute ("DataRate", DataRateValue (1000000));
  csma1.SetChannelAttribute ("Delay", TimeValue (MicroSeconds (2)));
  NetDeviceContainer devices1 = csma1.Install (NodeContainer (nodes.Get (1), router.Get (0)));

  InternetStackHelper internet;
  internet.Install (nodes);
  internet.Install (router);

  Ipv6AddressHelper ipv6;
  ipv6.SetBase (Ipv6Address ("2001:1::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer interfaces0 = ipv6.Assign (devices0);

  ipv6.SetBase (Ipv6Address ("2001:2::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer interfaces1 = ipv6.Assign (devices1);

  // Set up static routes on the router
  Ipv6StaticRoutingHelper staticRouting;
  Ptr<Ipv6StaticRouting> routerRouting = staticRouting.GetIpv6StaticRouting (router.Get (0)->GetObject<Ipv6> ());
  routerRouting->AddRoutingTableEntry (Ipv6Address ("2001:1::"), 64, interfaces0.GetAddress (1, 1), 1);
  routerRouting->AddRoutingTableEntry (Ipv6Address ("2001:2::"), 64, interfaces1.GetAddress (1, 1), 1);
  routerRouting->SetDefaultRoute (interfaces0.GetAddress (0, 1), 1);

  // Configure Radvd on the router
  RadvdHelper radvd;
  radvd.SetPrefix (Ipv6Prefix ("2001:1::/64"), interfaces0.GetAddress (1, 1));
  radvd.SetPrefix (Ipv6Prefix ("2001:2::/64"), interfaces1.GetAddress (1, 1));
  radvd.Install (router.Get (0));

  // Set up Ping application from n0 to n1
  V6PingHelper ping (interfaces1.GetAddress (0, 0));
  ping.SetAttribute ("Verbose", BooleanValue (true));
  ApplicationContainer apps = ping.Install (nodes.Get (0));
  apps.Start (Seconds (1.0));
  apps.Stop (Seconds (10.0));

  // Enable Tracing
  CsmaHelper::EnableAsciiAll ("radvd.tr");
  CsmaHelper::EnablePcapAll ("radvd");

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}