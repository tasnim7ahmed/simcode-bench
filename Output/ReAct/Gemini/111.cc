#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/ipv6-routing-table.h"
#include "ns3/radvd-module.h"
#include "ns3/icmpv6-echo.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("RadvdExample");

int main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);
  LogComponent::SetDefaultLogLevel(LogLevel::LOG_LEVEL_ALL);
  LogComponent::SetDefaultLogFlag(LogComponent::LOG_PREFIX | LogComponent::LOG_NODE | LogComponent::LOG_TIME);

  NodeContainer nodes;
  nodes.Create (2);

  NodeContainer router;
  router.Create (1);

  CsmaHelper csma0;
  csma0.SetChannelAttribute ("DataRate", DataRateValue (100 * 1000000));
  csma0.SetChannelAttribute ("Delay", TimeValue (MicroSeconds (2)));

  CsmaHelper csma1;
  csma1.SetChannelAttribute ("DataRate", DataRateValue (100 * 1000000));
  csma1.SetChannelAttribute ("Delay", TimeValue (MicroSeconds (2)));

  NetDeviceContainer leftDevices = csma0.Install (NodeContainer (router, nodes.Get (0)));
  NetDeviceContainer rightDevices = csma1.Install (NodeContainer (router, nodes.Get (1)));

  InternetStackHelper stack;
  stack.Install (nodes);
  stack.Install (router);

  Ipv6AddressHelper ipv6;
  ipv6.SetBase (Ipv6Address ("2001:1::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer leftIfaces = ipv6.Assign (leftDevices);

  ipv6.SetBase (Ipv6Address ("2001:2::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer rightIfaces = ipv6.Assign (rightDevices);

  Ptr<Ipv6StaticRouting> staticRouting = Ipv6RoutingHelper::GetRouting <Ipv6StaticRouting> (router.Get (0)->GetObject<Ipv6> ()->GetRoutingProtocol ());
  staticRouting->SetDefaultRoute (Ipv6Address ("fe80::" + leftIfaces.GetAddress (1, 1).ToString ()), 1);
  staticRouting->SetDefaultRoute (Ipv6Address ("fe80::" + rightIfaces.GetAddress (1, 1).ToString ()), 2);

  leftIfaces.SetForwarding (1, true);
  rightIfaces.SetForwarding (1, true);

  leftIfaces.SetDefaultRouteInAllNodes (Ipv6Address ("fe80::" + leftIfaces.GetAddress (0, 1).ToString ()));
  rightIfaces.SetDefaultRouteInAllNodes (Ipv6Address ("fe80::" + rightIfaces.GetAddress (0, 1).ToString ()));

  RadvdHelper radvd;
  radvd.SetAdvSendAdvert (true);

  ApplicationContainer apps = radvd.Install (router.Get (0));
  radvd.AddInterface (router.Get (0), 1, "2001:1::/64");
  radvd.AddInterface (router.Get (0), 2, "2001:2::/64");

  apps.Start (Seconds (1.0));
  apps.Stop (Seconds (10.0));

  V6Icmpv6EchoClientHelper ping6;
  ping6.SetRemote (Ipv6Address ("2001:2::2"));
  ping6.SetInterval (Seconds (1));
  ping6.SetSize (100);

  ApplicationContainer pingApps = ping6.Install (nodes.Get (0));
  pingApps.Start (Seconds (2.0));
  pingApps.Stop (Seconds (9.0));

  csma0.EnablePcap ("radvd", leftDevices.Get (0), true);
  csma1.EnablePcap ("radvd", rightDevices.Get (0), true);
  csma0.EnablePcap ("radvd", leftDevices.Get (1), true);
  csma1.EnablePcap ("radvd", rightDevices.Get (1), true);

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}