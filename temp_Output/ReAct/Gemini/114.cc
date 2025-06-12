#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/sixlowpan-module.h"
#include "ns3/lr-wpan-module.h"
#include "ns3/propagation-module.h"
#include "ns3/energy-module.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/icmpv6-echo.h"
#include "ns3/application.h"
#include "ns3/drop-tail-queue.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WsnIcmpv6");

int main (int argc, char *argv[])
{
  LogComponent::EnableIcmpv6 ();
  Time::SetResolution (Time::NS);

  bool verbose = true;
  if (verbose)
    {
      LogComponent::Enable ("WsnIcmpv6", LOG_LEVEL_ALL);
      LogComponent::Enable ("LrWpanMac", LOG_LEVEL_ALL);
    }

  NodeContainer nodes;
  nodes.Create (2);

  LrWpanHelper lrWpanHelper;
  NetDeviceContainer lrWpanDevices = lrWpanHelper.Install (nodes);

  SixLowPanHelper sixLowPanHelper;
  NetDeviceContainer sixLowPanDevices = sixLowPanHelper.Install (lrWpanDevices);

  InternetStackHelper internetv6;
  internetv6.Install (nodes);

  Ipv6AddressHelper ipv6;
  ipv6.SetBase (Ipv6Address ("2001:db8::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer interfaces = ipv6.Assign (sixLowPanDevices);

  interfaces.GetAddress (0, 1);
  interfaces.GetAddress (1, 1);
  interfaces.SetForwarding (0, true);
  interfaces.SetForwarding (1, true);

  Icmpv6EchoClientHelper ping6;
  ping6.SetRemote (interfaces.GetAddress (1, 1));
  ping6.SetInterval (Seconds (1));
  ping6.SetSize (32);

  ApplicationContainer apps = ping6.Install (nodes.Get (0));
  apps.Start (Seconds (2.0));
  apps.Stop (Seconds (10.0));

  Packet::EnableTracing ("wsn-ping6.tr");

  Simulator::Stop (Seconds (12.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}