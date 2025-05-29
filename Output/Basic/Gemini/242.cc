#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/sixlowpan-module.h"
#include "ns3/lr-wpan-module.h"
#include "ns3/udp-client-server-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/on-off-application.h"
#include "ns3/packet-sink.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("6LoWPANExample");

int main (int argc, char *argv[])
{
  bool verbose = false;

  CommandLine cmd;
  cmd.AddValue ("verbose", "Tell applications to log if set to true", verbose);
  cmd.Parse (argc,argv);

  if (verbose)
    {
      LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
      LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);
    }

  Time::SetResolution (Time::NS);

  NodeContainer nodes;
  nodes.Create (6); // 5 IoT devices + 1 Server

  LrWpanHelper lrWpanHelper;
  NetDeviceContainer lrWpanDevices = lrWpanHelper.Install (nodes);

  SixLowPanHelper sixLowPanHelper;
  NetDeviceContainer sixLowPanDevices = sixLowPanHelper.Install (lrWpanDevices);

  InternetStackHelper internetv6;
  internetv6.Install (nodes);

  Ipv6AddressHelper ipv6;
  ipv6.SetBase (Ipv6Address ("2001:db8::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer interfaces = ipv6.Assign (sixLowPanDevices);

  // Configure static routing for IPv6
  Ipv6StaticRoutingHelper ipv6RoutingHelper;
  Ptr<Ipv6StaticRouting> staticRouting = ipv6RoutingHelper.GetOrCreateRouting (nodes.Get (0));
  staticRouting->SetDefaultRoute (Ipv6Address ("2001:db8::1"), 0);

  for (uint32_t i = 1; i < nodes.GetN (); ++i)
    {
      Ptr<Ipv6StaticRouting> staticRoutingDevice = ipv6RoutingHelper.GetOrCreateRouting (nodes.Get (i));
      staticRoutingDevice->SetDefaultRoute (Ipv6Address ("2001:db8::"), 0);
    }

  UdpClientServerHelper udpClientServer (9);
  udpClientServer.SetAttribute ("MaxPackets", UintegerValue (1000));
  udpClientServer.SetAttribute ("Interval", TimeValue (Seconds (1.)));
  udpClientServer.SetAttribute ("PacketSize", UintegerValue (10));

  ApplicationContainer clientApps;
  for (uint32_t i = 1; i < nodes.GetN (); ++i)
    {
      udpClientServer.SetAttribute ("RemoteAddress", Ipv6AddressValue (interfaces.GetAddress (0, 1)));
      clientApps.Add (udpClientServer.Install (nodes.Get (i)));
    }

  ApplicationContainer serverApps = udpClientServer.Install (nodes.Get (0));

  clientApps.Start (Seconds (2.0));
  serverApps.Start (Seconds (1.0));
  clientApps.Stop (Seconds (10.0));
  serverApps.Stop (Seconds (10.0));

  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}