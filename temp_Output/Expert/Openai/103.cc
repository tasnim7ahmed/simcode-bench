#include "ns3/core-module.h"
#include "ns3/csma-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv6-address-helper.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/ipv6-routing-table-entry.h"
#include "ns3/ipv6-interface-container.h"
#include "ns3/ipv6-address.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main (int argc, char *argv[])
{
  // Create three nodes: n0, r, n1
  NodeContainer nodes;
  nodes.Create (3);

  NodeContainer n0r = NodeContainer (nodes.Get (0), nodes.Get (1));
  NodeContainer rn1 = NodeContainer (nodes.Get (1), nodes.Get (2));

  // Install Internet stack with IPv6
  InternetStackHelper internetv6;
  internetv6.Install (nodes);

  // Setup CSMA
  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", StringValue ("100Mbps"));
  csma.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (2)));

  NetDeviceContainer dev_n0r = csma.Install (n0r);
  NetDeviceContainer dev_rn1 = csma.Install (rn1);

  // Assign IPv6 addresses
  Ipv6AddressHelper ipv6;

  // n0 <-> r: fd00:1::/64
  ipv6.SetBase (Ipv6Address ("fd00:1::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer iface_n0r = ipv6.Assign (dev_n0r);
  iface_n0r.SetForwarding (1, true);
  iface_n0r.SetDefaultRouteInAllNodes (1);

  // r <-> n1: fd00:2::/64
  ipv6.SetBase (Ipv6Address ("fd00:2::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer iface_rn1 = ipv6.Assign (dev_rn1);
  iface_rn1.SetForwarding (0, true);

  // Configure static routing
  Ptr<Ipv6StaticRouting> staticRoutingN0 = Ipv6RoutingHelper::GetRouting <Ipv6StaticRouting> (nodes.Get (0)->GetObject<Ipv6> ());
  staticRoutingN0->SetDefaultRoute (iface_n0r.GetAddress (1, 1), 1);

  Ptr<Ipv6StaticRouting> staticRoutingN1 = Ipv6RoutingHelper::GetRouting <Ipv6StaticRouting> (nodes.Get (2)->GetObject<Ipv6> ());
  staticRoutingN1->SetDefaultRoute (iface_rn1.GetAddress (0, 1), 1);

  // Ping6 from n0 to n1
  uint32_t packetSize = 56;
  uint32_t maxPackets = 4;
  Time interPacketInterval = Seconds (1.0);

  Ping6Helper ping6;
  ping6.SetLocal (iface_n0r.GetAddress (0,1));
  ping6.SetRemote (iface_rn1.GetAddress (1,1));
  ping6.SetAttribute ("MaxPackets", UintegerValue (maxPackets));
  ping6.SetAttribute ("Interval", TimeValue (interPacketInterval));
  ping6.SetAttribute ("PacketSize", UintegerValue (packetSize));
  ping6.SetAttribute ("Verbose", BooleanValue (false));

  ApplicationContainer apps = ping6.Install (nodes.Get (0));
  apps.Start (Seconds (1.0));
  apps.Stop (Seconds (10.0));

  // Enable tracing
  csma.EnablePcapAll ("three-node-ipv6-csma", true);
  AsciiTraceHelper ascii;
  csma.EnableAsciiAll (ascii.CreateFileStream ("three-node-ipv6-csma.tr"));

  // Print IPv6 routing table of n0 at time 0.5s
  Ipv6RoutingHelper routingHelper;
  Simulator::Schedule (Seconds (0.5), &Ipv6RoutingHelper::PrintRoutingTableAt, &routingHelper, Seconds (0.5), nodes.Get (0));

  Simulator::Stop (Seconds (11.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}