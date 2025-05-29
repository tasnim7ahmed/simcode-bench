#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv6-address-helper.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/applications-module.h"
#include "ns3/ipv6-routing-table-entry.h"

using namespace ns3;

int
main (int argc, char *argv[])
{
  // Set simulation resolution and logging
  Time::SetResolution (Time::NS);

  // Create nodes: n0, router, n1
  NodeContainer nodes;
  nodes.Create (3);
  Ptr<Node> n0 = nodes.Get (0);
  Ptr<Node> r  = nodes.Get (1);
  Ptr<Node> n1 = nodes.Get (2);

  // Create two CSMA channels: n0 <-> r, r <-> n1
  NodeContainer n0r (n0, r);
  NodeContainer rn1 (r, n1);

  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", DataRateValue (DataRate ("100Mbps")));
  csma.SetChannelAttribute ("Delay", TimeValue (NanoSeconds (6560)));

  NetDeviceContainer dev_n0r = csma.Install (n0r);
  NetDeviceContainer dev_rn1 = csma.Install (rn1);

  // Install IPv6 stack
  InternetStackHelper internetv6;
  internetv6.SetIpv4StackInstall (false); // Only install IPv6 stack
  internetv6.Install (nodes);

  // Assign IPv6 addresses
  // Network 1: 2001:1::/64 (n0 <-> r)
  // Network 2: 2001:2::/64 (r <-> n1)
  Ipv6AddressHelper ipv6;
  ipv6.SetBase (Ipv6Address ("2001:1::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer iface_n0r = ipv6.Assign (dev_n0r);
  // By default, interfaces are set as not forwarding. Set r's n0r interface to forwarding.
  iface_n0r.SetForwarding (1, true);
  iface_n0r.SetDefaultRouteInAllNodes (0);

  ipv6.SetBase (Ipv6Address ("2001:2::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer iface_rn1 = ipv6.Assign (dev_rn1);
  // Set r's rn1 interface to forwarding.
  iface_rn1.SetForwarding (0, true);

  // Set the router node (r) to forward packets
  Ptr<Ipv6> ipv6_r = r->GetObject<Ipv6> ();
  ipv6_r->SetAttribute ("IpForward", BooleanValue (true));

  // Set up default routes:
  // n0: route to n1 via r's address on n0<->r
  Ptr<Ipv6StaticRouting> staticRouting_n0 = Ipv6RoutingHelper::GetRouting <Ipv6StaticRouting> (n0->GetObject<Ipv6> ());
  staticRouting_n0->SetDefaultRoute (iface_n0r.GetAddress (1, 1), 1);

  // n1: set default route via r's address on r<->n1
  Ptr<Ipv6StaticRouting> staticRouting_n1 = Ipv6RoutingHelper::GetRouting <Ipv6StaticRouting> (n1->GetObject<Ipv6> ());
  staticRouting_n1->SetDefaultRoute (iface_rn1.GetAddress (0, 1), 1);

  // Install V6 ping app (V6Ping is available in ns-3.37+)
  uint32_t packetSize = 56; // bytes
  uint32_t maxPackets = 5;
  Time interPacketInterval = Seconds (1.0);

#if (NS3_MAJOR_VERSION > 3) || (NS3_MAJOR_VERSION == 3 && NS3_MINOR_VERSION >= 37)
  V6PingHelper ping (iface_rn1.GetAddress (1, 1));
  ping.SetAttribute ("Verbose", BooleanValue (true));
  ping.SetAttribute ("PacketSize", UintegerValue (packetSize));
  ping.SetAttribute ("MaxPackets", UintegerValue (maxPackets));
  ping.SetAttribute ("Interval", TimeValue (interPacketInterval));
  ApplicationContainer apps = ping.Install (n0);
  apps.Start (Seconds (2.0));
  apps.Stop (Seconds (10.0));
#else
  // For ns-3 versions before 3.37, V6Ping is not available.
  NS_FATAL_ERROR ("V6PingHelper or V6PingApplication requires ns-3.37 or later.");
#endif

  // Enable ASCII and pcap tracing on both channels
  AsciiTraceHelper ascii;
  csma.EnableAsciiAll (ascii.CreateFileStream ("three-node-ipv6.tr"));
  csma.EnablePcapAll ("three-node-ipv6", true, true);

  // Print n0's routing table at time 2s
  Ipv6StaticRoutingHelper ipv6RoutingHelper;
  Simulator::Schedule (Seconds (2.0), &Ipv6StaticRoutingHelper::PrintRoutingTable, &ipv6RoutingHelper, n0, Create<OutputStreamWrapper> (&std::cout));

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}