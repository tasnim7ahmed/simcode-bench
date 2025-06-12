#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/ipv6-routing-table-entry.h"
#include "ns3/mobility-module.h"
#include "ns3/lr-wpan-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/config-store-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WsnPing6Simulation");

int
main (int argc, char *argv[])
{
  CommandLine cmd (__FILE__);
  cmd.Parse (argc, argv);

  LogComponentEnable ("Icmpv6L4Protocol", LOG_LEVEL_ALL);
  LogComponentEnable ("Ipv6Interface", LOG_LEVEL_ALL);
  LogComponentEnable ("WsnPing6Simulation", LOG_LEVEL_INFO);

  // Create nodes
  NodeContainer nodes;
  nodes.Create (2);

  // Setup mobility - stationary
  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (10.0),
                                 "DeltaY", DoubleValue (0),
                                 "GridWidth", UintegerValue (2),
                                 "LayoutType", StringValue ("RowFirst"));
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (nodes);

  // Setup LR-WPAN MAC and PHY
  LrWpanHelper lrWpanHelper;
  NetDeviceContainer devices = lrWpanHelper.Install (nodes);

  // Enable ASCII trace
  AsciiTraceHelper asciiTraceHelper;
  lrWpanHelper.EnableAsciiAll (asciiTraceHelper.CreateFileStream ("wsn-ping6.tr"));

  // Install protocol stack
  InternetStackHelper internetv6;
  internetv6.SetIpv4StackEnabled (false);
  internetv6.SetIpv6StackEnabled (true);
  internetv6.Install (nodes);

  // Assign IPv6 addresses
  Ipv6AddressHelper ipv6;
  Ipv6InterfaceContainer interfaces;
  ipv6.SetBase (Ipv6Address ("2001:1::"), Ipv6Prefix (64));
  interfaces = ipv6.Assign (devices);

  // Set up static routing
  Ipv6StaticRoutingHelper routingHelper;
  Ptr<Ipv6StaticRouting> routing0 = routingHelper.GetStaticRouting (nodes.Get (0)->GetObject<Ipv6> ());
  Ptr<Ipv6StaticRouting> routing1 = routingHelper.GetStaticRouting (nodes.Get (1)->GetObject<Ipv6> ());

  routing0->AddHostRouteTo (interfaces.GetAddress (1, 1), 1);
  routing1->AddHostRouteTo (interfaces.GetAddress (0, 1), 1);

  // Create Ping6 application
  V4PingHelper ping6 (interfaces.GetAddress (1, 1));
  ping6.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  ping6.SetAttribute ("Size", UintegerValue (64));
  ping6.SetAttribute ("Verbose", BooleanValue (true));

  ApplicationContainer apps = ping6.Install (nodes.Get (0));
  apps.Start (Seconds (1.0));
  apps.Stop (Seconds (10.0));

  // Enable logging of packet flows
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  Simulator::Stop (Seconds (11.0));
  Simulator::Run ();

  monitor->CheckForLostPackets ();
  flowmon.SerializeToXmlFile ("wsn-ping6.flowmon", false, false);

  Simulator::Destroy ();

  return 0;
}