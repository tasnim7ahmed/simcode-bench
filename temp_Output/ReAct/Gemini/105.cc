#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/ipv6-routing-table-entry.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("FragmentationIpv6");

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
      LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
      LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);
    }

  NodeContainer nodes;
  nodes.Create (3);

  NodeContainer n0r = NodeContainer (nodes.Get (0), nodes.Get (1));
  NodeContainer n1r = NodeContainer (nodes.Get (1), nodes.Get (2));

  CsmaHelper csma01;
  csma01.SetChannelAttribute ("DataRate", StringValue ("5Mbps"));
  csma01.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (2)));

  CsmaHelper csma12;
  csma12.SetChannelAttribute ("DataRate", StringValue ("5Mbps"));
  csma12.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (2)));

  NetDeviceContainer d01 = csma01.Install (n0r);
  NetDeviceContainer d12 = csma12.Install (n1r);

  InternetStackHelper internetv6;
  internetv6.Install (nodes);

  Ipv6AddressHelper ipv6;
  ipv6.SetBase (Ipv6Address ("2001:db8:0:1::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer i01 = ipv6.Assign (d01);
  ipv6.SetBase (Ipv6Address ("2001:db8:0:2::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer i12 = ipv6.Assign (d12);

  Ipv6StaticRoutingHelper ipv6RoutingHelper;
  Ptr<Ipv6StaticRouting> staticRouting = ipv6RoutingHelper.GetStaticRouting (nodes.Get (1)->GetObject<Ipv6> ());
  staticRouting->AddGlobalRoute (Ipv6Address ("2001:db8:0:1::/64"), 1, i01.GetAddress (0));
  staticRouting->AddGlobalRoute (Ipv6Address ("2001:db8:0:2::/64"), 2, i12.GetAddress (1));

  staticRouting = ipv6RoutingHelper.GetStaticRouting (nodes.Get (0)->GetObject<Ipv6> ());
  staticRouting->AddGlobalRoute (Ipv6Address ("2001:db8:0:2::/64"), 1, i01.GetAddress (1));

  staticRouting = ipv6RoutingHelper.GetStaticRouting (nodes.Get (2)->GetObject<Ipv6> ());
  staticRouting->AddGlobalRoute (Ipv6Address ("2001:db8:0:1::/64"), 1, i12.GetAddress (0));

  UdpEchoServerHelper echoServer (9);
  ApplicationContainer serverApps = echoServer.Install (nodes.Get (2));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  UdpEchoClientHelper echoClient (i12.GetAddress (1), 9);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (1));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (2000));
  ApplicationContainer clientApps = echoClient.Install (nodes.Get (0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  if (tracing)
    {
      csma01.EnablePcap ("fragmentation-ipv6", d01.Get (0), true);
      csma12.EnablePcap ("fragmentation-ipv6", d12.Get (0), true);
      csma01.EnablePcap ("fragmentation-ipv6", d01.Get (1), true);
      csma12.EnablePcap ("fragmentation-ipv6", d12.Get (1), true);
    }

  AsciiTraceHelper ascii;
  Ptr<OutputStreamWrapper> stream = ascii.CreateFileStream ("fragmentation-ipv6.tr");
  csma01.EnableAsciiAll (stream);
  csma12.EnableAsciiAll (stream);

  Simulator::Stop (Seconds (11.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}