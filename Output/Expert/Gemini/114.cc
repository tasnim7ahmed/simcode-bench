#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/ipv6-routing-table-entry.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/log.h"
#include "ns3/mobility-module.h"
#include "ns3/lr-wpan-module.h"
#include "ns3/sixlowpan-module.h"
#include "ns3/internet-apps-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WsnPing6");

int main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);
  LogComponent::SetLogLevel (LOG_LEVEL_INFO);
  LogComponent::Enable ("UdpClient", LOG_LEVEL_INFO);
  LogComponent::Enable ("UdpServer", LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create (2);

  LrWpanHelper lrWpanHelper;
  NetDeviceContainer lrWpanDevices = lrWpanHelper.Install (nodes);

  SixLowPanHelper sixLowPanHelper;
  NetDeviceContainer sixLowPanDevices = sixLowPanHelper.Install (lrWpanDevices);

  InternetStackHelper internet;
  internet.Install (nodes);

  Ipv6AddressHelper ipv6;
  ipv6.SetBase (Ipv6Address("2001:db8::"), Ipv6Prefix(64));
  Ipv6InterfaceContainer interfaces = ipv6.Assign (sixLowPanDevices);

  Ptr<Node> sender = nodes.Get (0);
  Ptr<Node> receiver = nodes.Get (1);

  Ptr<Ipv6StaticRouting> staticRouting = Ipv6RoutingHelper::GetRouting <Ipv6StaticRouting> (sender->GetObject<Ipv6> ()->GetRoutingProtocol ());
  staticRouting->SetDefaultRoute (interfaces.GetAddress (1, 1), 0);

  staticRouting = Ipv6RoutingHelper::GetRouting <Ipv6StaticRouting> (receiver->GetObject<Ipv6> ()->GetRoutingProtocol ());
  staticRouting->SetDefaultRoute (interfaces.GetAddress (0, 1), 0);

  V6PingHelper ping6 (interfaces.GetAddress (1, 1));
  ping6.SetAttribute ("Verbose", BooleanValue (true));
  ApplicationContainer apps = ping6.Install (nodes.Get (0));
  apps.Start (Seconds (1.0));
  apps.Stop (Seconds (10.0));

  Simulator::Stop (Seconds (11.0));

  AsciiTraceHelper ascii;
  PointToPointHelper pointToPoint;
  pointToPoint.EnableAsciiAll (ascii.CreateFileStream ("wsn-ping6.tr"));

  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}