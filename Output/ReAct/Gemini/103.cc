#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/ipv6-routing-table.h"
#include "ns3/ipv6-static-routing.h"
#include "ns3/ipv6-routing-helper.h"
#include "ns3/ping6.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("Ipv6CsmaExample");

int main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);
  LogComponent::SetDefaultLogLevel(LOG_LEVEL_INFO);
  LogComponent::SetDefaultLogFlag(LOG_PREFIX_TIME | LOG_PREFIX_NODE);

  NodeContainer nodes;
  nodes.Create (3);

  NodeContainer n0r = NodeContainer (nodes.Get (0), nodes.Get (1));
  NodeContainer rn1 = NodeContainer (nodes.Get (1), nodes.Get (2));

  CsmaHelper csma01;
  csma01.SetChannelAttribute ("DataRate", DataRateValue (DataRate ("5Mbps")));
  csma01.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (2)));
  NetDeviceContainer d0d1 = csma01.Install (n0r);

  CsmaHelper csma12;
  csma12.SetChannelAttribute ("DataRate", DataRateValue (DataRate ("5Mbps")));
  csma12.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (2)));
  NetDeviceContainer d1d2 = csma12.Install (rn1);

  InternetStackHelper internetv6;
  internetv6.Install (nodes);

  Ipv6AddressHelper ipv6;
  ipv6.SetBase (Ipv6Address ("2001:db8:0:1::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer i0i1 = ipv6.Assign (d0d1);

  ipv6.SetBase (Ipv6Address ("2001:db8:0:2::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer i1i2 = ipv6.Assign (d1d2);

  // Set IPv6 forwarding on the router
  Ipv6StaticRoutingHelper ipv6RoutingHelper;
  ipv6RoutingHelper.EnableStaticRouting (nodes.Get (1));

  // Add default routes
  Ptr<Ipv6StaticRouting> staticRouting = ipv6RoutingHelper.GetStaticRouting (nodes.Get (1));
  staticRouting->AddGlobalRoute (Ipv6Address ("2001:db8:0:1::"), 64, i0i1.GetAddress (0), 1);
  staticRouting->AddGlobalRoute (Ipv6Address ("2001:db8:0:2::"), 64, i1i2.GetAddress (1), 1);

  //Set default route on node n0
  Ptr<Ipv6StaticRouting> staticRoutingN0 = ipv6RoutingHelper.GetStaticRouting (nodes.Get (0));
  staticRoutingN0->SetDefaultRoute (i0i1.GetAddress (1), 1);

  //Set default route on node n1
  Ptr<Ipv6StaticRouting> staticRoutingN1 = ipv6RoutingHelper.GetStaticRouting (nodes.Get (2));
  staticRoutingN1->SetDefaultRoute (i1i2.GetAddress (0), 1);

  // Create and install Ping6 application on node n0 to ping node n1
  V6Ping6Helper ping6 (i1i2.GetAddress (1));
  ping6.SetAttribute ("Verbose", BooleanValue (true));
  ApplicationContainer p = ping6.Install (nodes.Get (0));
  p.Start (Seconds (1.0));
  p.Stop (Seconds (10.0));

  Simulator::Stop (Seconds (11.0));

  // Enable pcap tracing
  csma01.EnablePcap ("ipv6-csma", d0d1.Get (0), true);
  csma12.EnablePcap ("ipv6-csma", d1d2.Get (0), true);

  // Enable ascii tracing
  AsciiTraceHelper ascii;
  csma01.EnableAsciiAll (ascii.CreateFileStream ("ipv6-csma.tr"));

  Simulator::Run ();

  // Print routing table for n0
  Ptr<Ipv6RoutingTable> routingTable = ipv6RoutingHelper.GetRoutingTable (nodes.Get (0));
  routingTable->PrintRoutingTable (std::cout);

  Simulator::Destroy ();
  return 0;
}