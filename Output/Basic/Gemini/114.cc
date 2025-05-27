#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/sixlowpan-module.h"
#include "ns3/lr-wpan-module.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/ipv6-routing-table-entry.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/mobility-module.h"
#include "ns3/queue.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WsnPing6");

int main (int argc, char *argv[])
{
  bool verbose = true;
  bool tracing = true;

  CommandLine cmd;
  cmd.AddValue ("verbose", "Tell application to log if true.", verbose);
  cmd.AddValue ("tracing", "Enable pcap tracing.", tracing);

  cmd.Parse (argc,argv);

  if (verbose)
    {
      LogComponentEnable ("WsnPing6", LOG_LEVEL_INFO);
      LogComponentEnable ("Icmpv6L4Protocol", LOG_LEVEL_ALL);
      LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
      LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);
    }

  Time::SetResolution (Time::NS);

  NodeContainer nodes;
  nodes.Create (2);

  LrWpanHelper lrWpanHelper;
  NetDeviceContainer lrWpanDevices = lrWpanHelper.Install (nodes);

  SixLowPanHelper sixLowPanHelper;
  NetDeviceContainer sixLowPanDevices = sixLowPanHelper.Install (lrWpanDevices);

  InternetStackHelper internet;
  internet.Install (nodes);

  Ipv6AddressHelper ipv6;
  ipv6.SetBase (Ipv6Address ("2001:db8::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer interfaces = ipv6.Assign (sixLowPanDevices);

  // Set up static routing for node 1 to reach node 0
  Ptr<Ipv6StaticRouting> staticRouting = Ipv6RoutingHelper::GetRouting <Ipv6StaticRouting> (nodes.Get (1)->GetObject<Ipv6> ()->GetRoutingProtocol ());
  staticRouting->SetDefaultRoute (interfaces.GetAddress (0, 1), 1);

  // Set up static routing for node 0 to reach node 1
  staticRouting = Ipv6RoutingHelper::GetRouting <Ipv6StaticRouting> (nodes.Get (0)->GetObject<Ipv6> ()->GetRoutingProtocol ());
  staticRouting->SetDefaultRoute (interfaces.GetAddress (1, 1), 1);

  // Mobility model (stationary)
  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (5.0),
                                 "DeltaY", DoubleValue (5.0),
                                 "GridWidth", UintegerValue (3),
                                 "LayoutType", StringValue ("RowFirst"));
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (nodes);

  // ICMPv6 Echo Request/Reply (Ping6)
  V6PingHelper ping6 (interfaces.GetAddress (1, 1));
  ping6.SetAttribute ("Verbose", BooleanValue (true));
  ping6.SetAttribute ("Interval", TimeValue (Seconds (1)));
  ping6.SetAttribute ("Size", UintegerValue (32));
  ApplicationContainer ping6App = ping6.Install (nodes.Get (0));
  ping6App.Start (Seconds (2));
  ping6App.Stop (Seconds (10));

  if (tracing)
    {
       AsciiTraceHelper ascii;
       Ptr<OutputStreamWrapper> stream = ascii.CreateFileStream ("wsn-ping6.tr");
       lrWpanHelper.EnableAsciiAll (stream);
       Simulator::EnablePcapAll ("wsn-ping6");
    }

  Simulator::Stop (Seconds (11));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}