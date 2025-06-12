#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/ipv6-routing-table-entry.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/mobility-module.h"
#include "ns3/sixlowpan-module.h"
#include "ns3/lr-wpan-module.h"
#include "ns3/propagation-loss-model.h"
#include "ns3/constant-position-mobility-model.h"
#include "ns3/drop-tail-queue.h"
#include "ns3/ping6-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WsnPing6");

int main (int argc, char *argv[])
{
  bool verbose = true;

  CommandLine cmd;
  cmd.AddValue ("verbose", "Tell application to log if set to true", verbose);

  cmd.Parse (argc, argv);

  if (verbose)
    {
      LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
      LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);
      LogComponentEnable ("Ping6Application", LOG_LEVEL_INFO);
      LogComponentEnable ("LrWpanPhy", LOG_LEVEL_ALL);
      LogComponentEnable ("LrWpanMac", LOG_LEVEL_ALL);
      LogComponentEnable ("SixLowPanNetDevice", LOG_LEVEL_ALL);
      LogComponentEnable ("Ipv6L3Protocol", LOG_LEVEL_ALL);
      LogComponentEnable ("WsnPing6", LOG_LEVEL_ALL);
    }

  Time::SetResolution (Time::NS);

  NodeContainer nodes;
  nodes.Create (2);

  // Mobility model
  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (5.0),
                                 "DeltaY", DoubleValue (10.0),
                                 "GridWidth", UintegerValue (3),
                                 "LayoutType", StringValue ("RowFirst"));
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (nodes);

  // LR-WPAN
  LrWpanHelper lrWpanHelper;
  NetDeviceContainer lrWpanDevices = lrWpanHelper.Install (nodes);

  // 6LoWPAN
  SixLowPanHelper sixLowPanHelper;
  NetDeviceContainer sixLowPanDevices = sixLowPanHelper.Install (lrWpanDevices);

  // Internet stack
  InternetStackHelper internetv6;
  internetv6.Install (nodes);

  // IPv6 Addressing
  Ipv6AddressHelper ipv6;
  ipv6.SetBase (Ipv6Address ("2001:db8::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer interfaces = ipv6.Assign (sixLowPanDevices);
  interfaces.SetForwarding (0, true);
  interfaces.SetForwarding (1, true);
  interfaces.SetDefaultRouteInAllNodes (0);

  // Set up static routes
  Ipv6StaticRoutingHelper ipv6RoutingHelper;
  Ptr<Ipv6StaticRouting> staticRouting = ipv6RoutingHelper.GetStaticRouting (nodes.Get (1)->GetObject<Ipv6> ());
  staticRouting->AddGlobalRouteTo (Ipv6Address ("2001:db8::1"), 0); // Route to node 0

  // Configure DropTail queue
  Config::SetDefault ("ns3::DropTailQueue::MaxPackets", UintegerValue (20));

  // Ping6
  Ping6Helper ping6;
  ping6.SetRemote (interfaces.GetAddress (1, 1));
  ping6.SetIfIndex (1); // outbound interface
  ping6.SetAttribute ("Verbose", BooleanValue (true));

  ApplicationContainer pings = ping6.Install (nodes.Get (0));
  pings.Start (Seconds (1.0));
  pings.Stop (Seconds (10.0));

  // Tracing
  PointToPointHelper pointToPoint;
  pointToPoint.EnablePcapAll ("wsn-ping6"); //deprecated but works
  // alternative, less verbose, tracing
  // lrWpanHelper.EnablePcap ("wsn-ping6", lrWpanDevices);
  // sixLowPanHelper.EnablePcap ("wsn-ping6", sixLowPanDevices);

  AsciiTraceHelper ascii;
  pointToPoint.EnableAsciiAll (ascii.CreateFileStream ("wsn-ping6.tr"));

  // Animation
  AnimationInterface anim ("wsn-ping6.xml");
  anim.SetConstantPosition (nodes.Get (0), 1.0, 1.0);
  anim.SetConstantPosition (nodes.Get (1), 10.0, 10.0);

  Simulator::Stop (Seconds (11.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}