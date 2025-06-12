/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * radvd-two-prefix.cc
 *
 *  ns-3 simulation: Two hosts (n0 and n1) connected through a router (R)
 *  via CSMA links. Router sends RAs with multiple prefixes.
 *  n0 and n1 assigned IPv6 addresses automatically via SLAAC.
 *  n0 pings n1 using ICMPv6 echo.
 *  Tracing enabled for queues and packet receptions, results saved to "radvd-two-prefix.tr".
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv6-static-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("RadvdTwoPrefixExample");

void
PrintAddresses (Ptr<Node> router, Ptr<Node> n0, Ptr<Node> n1)
{
  NS_LOG_INFO ("=== IPv6 Interfaces and Addresses ===");
  Ptr<Ipv6> ipv6Router = router->GetObject<Ipv6> ();
  Ptr<Ipv6> ipv6n0 = n0->GetObject<Ipv6> ();
  Ptr<Ipv6> ipv6n1 = n1->GetObject<Ipv6> ();
  
  // Router's interfaces
  for (uint32_t i = 0; i < ipv6Router->GetNInterfaces (); ++i)
    {
      std::cout << "Router interface " << i << ": ";
      for (uint32_t j = 0; j < ipv6Router->GetNAddresses (i); ++j)
        {
          std::cout << ipv6Router->GetAddress (i, j).GetAddress () << " ";
        }
      std::cout << std::endl;
    }
  // n0's interfaces
  for (uint32_t i = 0; i < ipv6n0->GetNInterfaces (); ++i)
    {
      std::cout << "n0 interface " << i << ": ";
      for (uint32_t j = 0; j < ipv6n0->GetNAddresses (i); ++j)
        {
          std::cout << ipv6n0->GetAddress (i, j).GetAddress () << " ";
        }
      std::cout << std::endl;
    }
  // n1's interfaces
  for (uint32_t i = 0; i < ipv6n1->GetNInterfaces (); ++i)
    {
      std::cout << "n1 interface " << i << ": ";
      for (uint32_t j = 0; j < ipv6n1->GetNAddresses (i); ++j)
        {
          std::cout << ipv6n1->GetAddress (i, j).GetAddress () << " ";
        }
      std::cout << std::endl;
    }
  std::cout << "=====================================" << std::endl;
}

void
ReceivePacket (Ptr<const Packet> p, const Address &srcAddress)
{
  NS_LOG_UNCOND ("Received packet of size " << p->GetSize () << " bytes");
}

int
main (int argc, char *argv[])
{
  LogComponentEnable ("RadvdTwoPrefixExample", LOG_LEVEL_INFO);
  LogComponentEnable ("Ipv6L3Protocol", LOG_PREFIX_TIME);
  LogComponentEnable ("Ipv6AutoconfiguredPrefix", LOG_LEVEL_WARN);

  // 1. Nodes: n0--csma--R--csma--n1
  NodeContainer hosts;
  hosts.Create (2); // n0 = hosts.Get(0), n1 = hosts.Get(1)
  Ptr<Node> n0 = hosts.Get (0);
  Ptr<Node> n1 = hosts.Get (1);
  Ptr<Node> router = CreateObject<Node> ();

  // 2. CSMA Links
  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", StringValue ("100Mbps"));
  csma.SetChannelAttribute ("Delay", TimeValue (NanoSeconds (6560)));

  // Left LAN: n0 <-> router
  NodeContainer lan1;
  lan1.Add (n0);
  lan1.Add (router);

  NetDeviceContainer dev_lan1 = csma.Install (lan1);

  // Right LAN: router <-> n1
  NodeContainer lan2;
  lan2.Add (router);
  lan2.Add (n1);

  NetDeviceContainer dev_lan2 = csma.Install (lan2);

  // 3. Internet Stack with Ipv6 + ICMPv6
  InternetStackHelper internetv6;
  internetv6.Install (hosts);
  internetv6.Install (router);

  // 4. Ipv6 Address Assignment
  Ipv6AddressHelper ipv6_1;
  ipv6_1.SetBase (Ipv6Address ("2001:1::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer iface_lan1 = ipv6_1.Assign (dev_lan1);

  // For the *second* prefix on the left LAN (n0<->router)
  Ipv6AddressHelper ipv6_1b;
  ipv6_1b.SetBase (Ipv6Address ("2001:ABCD::"), Ipv6Prefix (64));
  // AddAddress() ensures an *additional* address is placed for each device
  for (uint32_t i = 0; i < dev_lan1.GetN (); ++i)
    ipv6_1b.NewAddress (dev_lan1.Get (i));

  // Right LAN
  Ipv6AddressHelper ipv6_2;
  ipv6_2.SetBase (Ipv6Address ("2001:2::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer iface_lan2 = ipv6_2.Assign (dev_lan2);

  // Set all interfaces (except router) to not forward, router to forward
  for (uint32_t i = 0; i < hosts.GetN (); ++i)
    {
      Ptr<Ipv6> ipv6 = hosts.Get (i)->GetObject<Ipv6> ();
      for (uint32_t j = 0; j < ipv6->GetNInterfaces (); ++j)
        {
          ipv6->SetForwarding (j, false);
        }
    }
  Ptr<Ipv6> ipv6r = router->GetObject<Ipv6> ();
  for (uint32_t j = 0; j < ipv6r->GetNInterfaces (); ++j)
    {
      ipv6r->SetForwarding (j, true);
    }

  // 5. Configure router to send Router Advertisements to both LANs
  // ns-3 does this automatically if Forwarding is enabled on the interface, and useSlaac is set for interfaces.
  // We must ensure router advertises *both* prefixes on iface_lan1
  // Assign both prefixes to router's left LAN interface
  uint32_t routerIf_lan1 = 1; // 0=n0, 1=router in lan1
  uint32_t routerIf_lan2 = 0; // 0=router, 1=n1 in lan2

  // Add second prefix to router's left interface (2001:ABCD::/64)
  ipv6r->AddAddress (routerIf_lan1, Ipv6InterfaceAddress (Ipv6Address ("2001:ABCD::1"), Ipv6Prefix (64)));

  // Router's left interface has two addresses
  // Manually set the RA configuration to advertise both prefixes.
  ipv6r->SetRouterAdvertisementStatus (routerIf_lan1, true);
  ipv6r->SetRouterAdvertisementStatus (routerIf_lan2, true);

  // 6. Tracing
  AsciiTraceHelper ascii;
  Ptr<OutputStreamWrapper> stream = ascii.CreateFileStream ("radvd-two-prefix.tr");
  csma.EnableAsciiAll (stream);

  // Trace device queue drops
  for (uint32_t i = 0; i < dev_lan1.GetN (); ++i)
    {
      Ptr<NetDevice> dev = dev_lan1.Get (i);
      Ptr<CsmaNetDevice> csmaDev = DynamicCast<CsmaNetDevice> (dev);
      if (csmaDev && csmaDev->GetQueue ())
        {
          csmaDev->GetQueue ()->TraceConnectWithoutContext ("Drop", MakeBoundCallback (
            [](Ptr<OutputStreamWrapper> file, Ptr<const Packet> p) {
                *file->GetStream () << "Packet dropped: " << *p << std::endl;
            }, stream));
        }
    }
  for (uint32_t i = 0; i < dev_lan2.GetN (); ++i)
    {
      Ptr<NetDevice> dev = dev_lan2.Get (i);
      Ptr<CsmaNetDevice> csmaDev = DynamicCast<CsmaNetDevice> (dev);
      if (csmaDev && csmaDev->GetQueue ())
        {
          csmaDev->GetQueue ()->TraceConnectWithoutContext ("Drop", MakeBoundCallback (
            [](Ptr<OutputStreamWrapper> file, Ptr<const Packet> p) {
                *file->GetStream () << "Packet dropped: " << *p << std::endl;
            }, stream));
        }
    }

  // Trace packet reception at n1
  dev_lan2.Get (1)->TraceConnectWithoutContext ("PhyRxEnd", MakeCallback (&ReceivePacket));

  // Schedule printing addresses after DAD and autoconf settle (say at 4s)
  Simulator::Schedule (Seconds (4.0), &PrintAddresses, router, n0, n1);

  // 7. ICMPv6 Ping: n0 -> n1
  // Let n1 pick auto SLAAC address on right LAN
  Ipv6Address n1addr = iface_lan2.GetAddress (1, 1); // (iface, address 1 = global SLAAC)
  NS_LOG_INFO ("n1 global address: " << n1addr);

  V6PingHelper ping6 (n1addr);
  ping6.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  ping6.SetAttribute ("Size", UintegerValue (64));
  ping6.SetAttribute ("Count", UintegerValue (4));
  ApplicationContainer pings = ping6.Install (n0);
  pings.Start (Seconds (5.0));
  pings.Stop (Seconds (15.0));
  
  // Run simulation
  Simulator::Stop (Seconds (20.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}