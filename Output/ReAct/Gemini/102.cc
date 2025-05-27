#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/on-off-helper.h"
#include "ns3/packet-sink-helper.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/net-device-container.h"
#include "ns3/address.h"
#include "ns3/address-utils.h"
#include "ns3/data-rate.h"
#include "ns3/packet.h"
#include "ns3/uinteger.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("MulticastExample");

int main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);
  LogComponent::SetDefaultLogLevel (LOG_LEVEL_INFO);
  LogComponent::SetDefaultLogComponentEnable (true);

  NodeContainer nodes;
  nodes.Create (5);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer devices;
  for (uint32_t i = 0; i < 5; ++i) {
    for (uint32_t j = i + 1; j < 5; ++j) {
      devices.Add (p2p.Install (nodes.Get (i), nodes.Get (j)));
    }
  }

  InternetStackHelper internet;
  internet.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // Blacklist links
  Ipv4GlobalRoutingHelper::UninstallAllRouters (nodes.Get (0));
  Ipv4GlobalRoutingHelper::UninstallAllRouters (nodes.Get (1));
  Ipv4GlobalRoutingHelper::UninstallAllRouters (nodes.Get (2));
  Ipv4GlobalRoutingHelper::UninstallAllRouters (nodes.Get (3));
  Ipv4GlobalRoutingHelper::UninstallAllRouters (nodes.Get (4));

  Ipv4StaticRoutingHelper staticRouting;
  Ptr<Ipv4StaticRouting> routingA = staticRouting.GetStaticRouting (nodes.Get (0)->GetObject<Ipv4> (0));
  routingA->AddMulticastRoute (Ipv4Address ("224.0.0.1"), 0, Ipv4Address ("10.1.1.2"));
  routingA->AddMulticastRoute (Ipv4Address ("224.0.0.1"), 0, Ipv4Address ("10.1.1.3"));
  routingA->AddMulticastRoute (Ipv4Address ("224.0.0.1"), 0, Ipv4Address ("10.1.1.4"));
  routingA->AddMulticastRoute (Ipv4Address ("224.0.0.1"), 0, Ipv4Address ("10.1.1.5"));

  routingA->SetDefaultTtl (1);

  OnOffHelper onOffHelper ("ns3::UdpSocketFactory",
                           Address (InetSocketAddress (Ipv4Address ("224.0.0.1"), 9)));
  onOffHelper.SetConstantRate (DataRate ("1Mbps"));
  onOffHelper.SetAttribute ("PacketSize", UintegerValue (1024));

  ApplicationContainer clientApps = onOffHelper.Install (nodes.Get (0));
  clientApps.Start (Seconds (1.0));
  clientApps.Stop (Seconds (10.0));

  PacketSinkHelper sinkHelper ("ns3::UdpSocketFactory",
                               Address (InetSocketAddress (Ipv4Address::GetAny (), 9)));
  ApplicationContainer sinkApps;
  for (uint32_t i = 1; i < 5; ++i) {
    sinkApps.Add (sinkHelper.Install (nodes.Get (i)));
  }
  sinkApps.Start (Seconds (0.0));
  sinkApps.Stop (Seconds (11.0));

  p2p.EnablePcapAll ("multicast", false);

  Simulator::Stop (Seconds (11.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}