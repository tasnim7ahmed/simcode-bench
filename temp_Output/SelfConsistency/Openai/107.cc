/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Simulation: IPv6 fragmentation with two MTU links using CSMA
 * Topology:
 *
 *     n0 ----- r ----- n1
 *
 * n0---|5000|---r---|1500|---n1
 *
 * UDP echo from n0 to n1 through r
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv6-address-helper.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/ipv6-routing-table-entry.h"
#include "ns3/packet.h"
#include "ns3/queue.h"
#include "ns3/queue-size.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("FragmentationIpv6TwoMtuExample");

void
PacketReceiveCallback (Ptr<const Packet> packet, const Address &from)
{
  NS_LOG_INFO ("Packet received at " << Simulator::Now ().GetSeconds () << "s size=" << packet->GetSize ());
}

int
main (int argc, char *argv[])
{
  LogComponentEnable ("FragmentationIpv6TwoMtuExample", LOG_LEVEL_INFO);

  // Create nodes: n0, r, n1
  NodeContainer nodes;
  nodes.Create (3); // n0:0, r:1, n1:2

  // Create CSMA links for n0-r and r-n1
  NodeContainer n0r;
  n0r.Add (nodes.Get (0)); // n0
  n0r.Add (nodes.Get (1)); // r

  NodeContainer rn1;
  rn1.Add (nodes.Get (1)); // r
  rn1.Add (nodes.Get (2)); // n1

  // CSMA helper with default parameters
  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", DataRateValue (DataRate ("10Mbps")));
  csma.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (1)));

  // Set MTU for n0-r link
  csma.SetDeviceAttribute ("Mtu", UintegerValue (5000));
  NetDeviceContainer devN0R = csma.Install (n0r);

  // Set MTU for r-n1 link
  csma.SetDeviceAttribute ("Mtu", UintegerValue (1500));
  NetDeviceContainer devRN1 = csma.Install (rn1);

  // Install IPv6 stacks
  InternetStackHelper internetv6;
  internetv6.InstallAll ();

  // Assign IPv6 addresses
  Ipv6AddressHelper ipv6;
  // n0-r
  ipv6.SetBase (Ipv6Address ("2001:1::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer ifN0R = ipv6.Assign (devN0R);
  // Make interfaces up (except r's interface to n1 for now)
  ifN0R.SetForwarding (1, true);
  ifN0R.SetDefaultRouteInAllNodes (0);

  // r-n1
  ipv6.SetBase (Ipv6Address ("2001:2::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer ifRN1 = ipv6.Assign (devRN1);
  ifRN1.SetForwarding (0, true);
  ifRN1.SetDefaultRouteInAllNodes (1);

  // Set n1 as forwarding too (not needed, but in general)
  ifRN1.SetForwarding (1, false);

  // Set static routing:
  Ptr<Ipv6StaticRouting> staticRoutingN0 = Ipv6RoutingHelper::GetRouting <Ipv6StaticRouting> (nodes.Get (0));
  staticRoutingN0->SetDefaultRoute (ifN0R.GetAddress (1, 1), 1);

  Ptr<Ipv6StaticRouting> staticRoutingN1 = Ipv6RoutingHelper::GetRouting <Ipv6StaticRouting> (nodes.Get (2));
  staticRoutingN1->SetDefaultRoute (ifRN1.GetAddress (0, 1), 1);

  // Install UDP echo applications
  uint16_t echoPort = 9;

  // UDP echo server on n1
  UdpEchoServerHelper echoServer (echoPort);
  ApplicationContainer serverApps = echoServer.Install (nodes.Get (2));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  // UDP echo client on n0 (send to n1)
  UdpEchoClientHelper echoClient (ifRN1.GetAddress (1, 1), echoPort);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (3));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (4000));
  ApplicationContainer clientApps = echoClient.Install (nodes.Get (0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (9.0));

  // Tracing queues and reception to file
  AsciiTraceHelper ascii;
  Ptr<OutputStreamWrapper> stream = ascii.CreateFileStream ("fragmentation-ipv6-two-mtu.tr");
  csma.EnableAsciiAll (stream);

  // Trace packet receptions at n1
  devRN1.Get (1)->TraceConnectWithoutContext (
      "MacRx",
      MakeBoundCallback (
          [](Ptr<OutputStreamWrapper> stream, Ptr<const Packet> packet)
          {
            *stream->GetStream () << Simulator::Now ().GetSeconds ()
                                 << "s Received packet of size="
                                 << packet->GetSize ()
                                 << " bytes at n1" << std::endl;
          },
          stream));

  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}