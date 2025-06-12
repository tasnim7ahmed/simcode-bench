#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/ipv6-routing-table.h"
#include "ns3/ping6.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include <iostream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("Ipv6CsmaExample");

int main (int argc, char *argv[])
{
  bool verbose = false;
  bool tracing = true;

  CommandLine cmd;
  cmd.AddValue ("verbose", "Tell application to log if true", verbose);
  cmd.AddValue ("tracing", "Enable pcap tracing", tracing);

  cmd.Parse (argc, argv);

  if (verbose)
    {
      LogComponentEnable ("Ipv6CsmaExample", LOG_LEVEL_INFO);
      LogComponentEnable ("Ping6Application", LOG_LEVEL_INFO);
    }

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

  Ipv6StaticRoutingHelper ipv6RoutingHelper;
  Ptr<Ipv6StaticRouting> staticRouting = ipv6RoutingHelper.GetOrCreateRouting (nodes.Get (1)->GetObject<Ipv6> ());
  staticRouting->AddHostRouteTo (Ipv6Address ("2001:db8:0:1::1"), 1, i1i2.GetInterface (0)->GetIpv6 ()->GetAddress (1).GetScope ());
  staticRouting->AddHostRouteTo (Ipv6Address ("2001:db8:0:2::2"), 2, i0i1.GetInterface (1)->GetIpv6 ()->GetAddress (1).GetScope ());

  staticRouting = ipv6RoutingHelper.GetOrCreateRouting (nodes.Get (0)->GetObject<Ipv6> ());
  staticRouting->SetDefaultRoute (i0i1.GetAddress (1), 1, i0i1.GetInterface (1)->GetIpv6 ()->GetAddress (1).GetScope ());

  staticRouting = ipv6RoutingHelper.GetOrCreateRouting (nodes.Get (2)->GetObject<Ipv6> ());
  staticRouting->SetDefaultRoute (i1i2.GetAddress (1), 2, i1i2.GetInterface (1)->GetIpv6 ()->GetAddress (1).GetScope ());

  Ping6 ping6 (nodes.Get (2)->GetObject<Ipv6> (), Ipv6Address ("2001:db8:0:2::2"));
  ping6.SetAttribute ("Verbose", BooleanValue (true));
  ping6.SetAttribute ("Interval", TimeValue (Seconds (1)));
  ping6.SetAttribute ("Timeout", TimeValue (Seconds (10)));
  ping6.SetAttribute ("MaxPackets", UintegerValue (5));
  ApplicationContainer apps;
  apps.Add (ping6);
  apps.Start (Seconds (1.0));
  apps.Stop (Seconds (10.0));

  if (tracing)
    {
      AsciiTraceHelper ascii;
      csma01.EnableAsciiAll (ascii.CreateFileStream ("ipv6-csma.tr"));
      csma12.EnableAsciiAll (ascii.CreateFileStream ("ipv6-csma.tr"));
      csma01.EnablePcapAll ("ipv6-csma", false);
      csma12.EnablePcapAll ("ipv6-csma", false);
    }

  Simulator::Stop (Seconds (11.0));

  Simulator::Run ();

  Ptr<Ipv6RoutingTable> routingTable = nodes.Get (0)->GetObject<Ipv6> ()->GetRoutingTable ();
  std::cout << "Routing table for node n0:" << std::endl;
  routingTable->PrintRoutingTable (std::cout);

  Simulator::Destroy ();
  return 0;
}