#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/ipv6-routing-table-entry.h"
#include "ns3/applications-module.h"
#include "ns3/log.h"
#include "ns3/netanim-module.h"
#include "ns3/ipv6-extension-header.h"
#include "ns3/traffic-control-module.h"
#include <iostream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("FragmentationIpv6Simulation");

int main (int argc, char *argv[])
{
  bool enablePcap = true;

  CommandLine cmd;
  cmd.AddValue ("EnablePcap", "Enable or disable pcap tracing", enablePcap);
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);
  LogComponent::Enable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
  LogComponent::Enable ("UdpEchoServerApplication", LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create (3);

  NodeContainer n0r = NodeContainer (nodes.Get (0), nodes.Get (1));
  NodeContainer n1r = NodeContainer (nodes.Get (1), nodes.Get (2));

  CsmaHelper csma01;
  csma01.SetChannelAttribute ("DataRate", DataRateValue (DataRate ("5Mbps")));
  csma01.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (2)));
  NetDeviceContainer d0d1 = csma01.Install (n0r);

  CsmaHelper csma12;
  csma12.SetChannelAttribute ("DataRate", DataRateValue (DataRate ("5Mbps")));
  csma12.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (2)));
  NetDeviceContainer d1d2 = csma12.Install (n1r);

  InternetStackHelper internetv6;
  internetv6.Install (nodes);

  Ipv6AddressHelper ipv6;
  ipv6.SetBase (Ipv6Address ("2001:db8:0:1::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer i0i1 = ipv6.Assign (d0d1);

  ipv6.SetBase (Ipv6Address ("2001:db8:0:2::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer i1i2 = ipv6.Assign (d1d2);

  Ipv6StaticRoutingHelper ipv6RoutingHelper;

  auto staticRouting = ipv6RoutingHelper.GetStaticRouting (nodes.Get (0)->GetObject<Ipv6> ());
  staticRouting->AddGlobalRouteTo (Ipv6Address ("2001:db8:0:2::/64"), i0i1.GetAddress (1, 0), 1);

  staticRouting = ipv6RoutingHelper.GetStaticRouting (nodes.Get (2)->GetObject<Ipv6> ());
  staticRouting->AddGlobalRouteTo (Ipv6Address ("2001:db8:0:1::/64"), i1i2.GetAddress (0, 0), 1);

  UdpEchoServerHelper echoServer (9);

  ApplicationContainer serverApps = echoServer.Install (nodes.Get (2));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  UdpEchoClientHelper echoClient (i1i2.GetAddress (1), 9);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (1));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (2000));

  ApplicationContainer clientApps = echoClient.Install (nodes.Get (0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  if (enablePcap)
  {
    csma01.EnablePcap ("fragmentation-ipv6", d0d1.Get (0), true);
    csma12.EnablePcap ("fragmentation-ipv6", d1d2.Get (0), true);
  }

  Simulator::Stop (Seconds (11.0));

  AsciiTraceHelper ascii;
  csma01.EnableAsciiAll (ascii.CreateFileStream ("fragmentation-ipv6.tr"));
  csma12.EnableAsciiAll (ascii.CreateFileStream ("fragmentation-ipv6.tr"));

  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}