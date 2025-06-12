#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/ipv6-list-routing-helper.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("Icmpv6RedirectSimulation");

void
SendIpv6Redirect (Ptr<Node> r1, Ipv6Address sta1Addr, Ipv6Address sta2Addr, Ipv6Address r2Addr, 
                  uint32_t r1if1, uint32_t r1if2)
{
  // Manually construct an ICMPv6 Redirect and inject it into STA1
  Ptr<Packet> p = Create<Packet> ();

  uint8_t icmpType = 137; // ICMPv6 Redirect
  uint8_t icmpCode = 0;
  uint32_t reserved = 0;
  uint8_t redirectHdr[40];
  // Format: type, code, checksum, reserved, target address, destination address
  memset (redirectHdr, 0, sizeof(redirectHdr));
  redirectHdr[0] = icmpType;
  redirectHdr[1] = icmpCode;
  // leave checksum zero for ns-3, skip to reserved
  redirectHdr[4] = 0; redirectHdr[5] = 0; redirectHdr[6] = 0; redirectHdr[7] = 0;
  // target address: R2's address on R1-R2 link
  memcpy (&redirectHdr[8], r2Addr.GetBytes (), 16);
  // destination address: STA2's address
  memcpy (&redirectHdr[24], sta2Addr.GetBytes (), 16);

  Ptr<Packet> p2 = Create<Packet> ((uint8_t*)redirectHdr, 40);
  p->AddAtEnd (p2);

  // Build Ipv6Header (not standard in ns-3 injection but required for Receive)
  Ipv6Header ipv6Header;
  ipv6Header.SetSourceAddress (r1->GetObject<Ipv6> ()->GetAddress (r1if1, 1).GetAddress ()); // R1's address on R1-STA1 link
  ipv6Header.SetDestinationAddress (sta1Addr); // to STA1
  ipv6Header.SetPayloadLength (p->GetSize ());
  ipv6Header.SetNextHeader (Ipv6Header::IPV6_ICMPV6);
  ipv6Header.SetHopLimit (255);

  p->AddHeader (ipv6Header);

  // Deliver to STA1 node via its NetDevice input
  Ptr<NetDevice> nd = r1->GetDevice (r1if1);
  Ptr<Channel> channel = nd->GetChannel ();
  for (uint32_t i = 0; i < channel->GetNDevices (); ++i)
    {
      Ptr<NetDevice> dev = channel->GetDevice (i);
      Ptr<Node> node = dev->GetNode ();
      if (node != r1 && node->GetObject<Ipv6> ()->GetAddress (1,1).GetAddress () == sta1Addr)
        {
          dev->Receive (p, Ipv6L3Protocol::PROT_NUMBER, nd->GetAddress (), dev->GetAddress (), NetDevice::PACKET_HOST);
          break;
        }
    }
}

void
PacketTrace (Ptr<const Packet> p)
{
  static std::ofstream traceFile ("icmpv6-redirect.tr", std::ios::app);
  traceFile << Simulator::Now ().GetSeconds () << " " << *p << std::endl;
}

int
main (int argc, char *argv[])
{
  LogComponentEnable ("Icmpv6RedirectSimulation", LOG_LEVEL_INFO);

  NodeContainer routers, stas;
  routers.Create (2); // R1 (0), R2 (1)
  stas.Create (2);    // STA1 (0), STA2 (1)

  // Links: [STA1 - R1], [R1 - R2], [R2 - STA2]
  NodeContainer n_sta1r1 (stas.Get(0), routers.Get(0));
  NodeContainer n_r1r2 (routers.Get(0), routers.Get(1));
  NodeContainer n_r2sta2 (routers.Get(1), stas.Get(1));

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));
  NetDeviceContainer d_sta1r1 = p2p.Install (n_sta1r1);
  NetDeviceContainer d_r1r2 = p2p.Install (n_r1r2);
  NetDeviceContainer d_r2sta2 = p2p.Install (n_r2sta2);

  // Install the stack
  InternetStackHelper stack;
  Ipv6ListRoutingHelper listRH;
  Ipv6StaticRoutingHelper staticRH;
  listRH.Add (staticRH, 0);
  stack.SetIpv6StackInstall (true);
  stack.SetRoutingHelper (listRH);
  stack.Install (NodeContainer (routers, stas));

  // Assign IPv6 addresses (unique subnets)
  Ipv6AddressHelper ipv6;
  ipv6.SetBase (Ipv6Address ("2001:db8:1::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer if_sta1r1 = ipv6.Assign (d_sta1r1);
  ipv6.NewNetwork ();
  Ipv6InterfaceContainer if_r1r2 = ipv6.Assign (d_r1r2);
  ipv6.NewNetwork ();
  Ipv6InterfaceContainer if_r2sta2 = ipv6.Assign (d_r2sta2);

  if_sta1r1.SetForwarding(1, true); // R1 forward
  if_r1r2.SetForwarding(0, true); // R1
  if_r1r2.SetForwarding(1, true); // R2
  if_r2sta2.SetForwarding(0, true); // R2

  if_sta1r1.SetDefaultRouteInAllNodes (0); // For STA1, via R1
  if_r2sta2.SetDefaultRouteInAllNodes (1); // For STA2, via R2

  // Remove default routing from routers
  Ptr<Ipv6StaticRouting> r1Static = Ipv6RoutingHelper::GetRouting <Ipv6StaticRouting> (routers.Get(0)->GetObject<Ipv6> ()->GetRoutingProtocol ());
  Ptr<Ipv6StaticRouting> r2Static = Ipv6RoutingHelper::GetRouting <Ipv6StaticRouting> (routers.Get(1)->GetObject<Ipv6> ()->GetRoutingProtocol ());

  // For R1: Add route to STA2 via R2 (next hop: R2's interface on R1-R2 link)
  r1Static->AddNetworkRouteTo (if_r2sta2.GetAddress(1,1), Ipv6Prefix(128),
                               if_r1r2.GetAddress(1,1), if_r1r2.GetInterfaceIndex(0));

  // For R2: Add route to STA2's interface via direct link
  r2Static->AddNetworkRouteTo (if_r2sta2.GetAddress(1,1), Ipv6Prefix(128),
                               if_r2sta2.GetAddress(1,0), if_r2sta2.GetInterfaceIndex(0));

  // For tracing ICMPv6
  Config::ConnectWithoutContext ("/NodeList/*/ApplicationList/*/$ns3::PacketSink/Rx", MakeCallback(&PacketTrace));
  Config::ConnectWithoutContext ("/NodeList/*/Ipv6L3Protocol/Rx", MakeCallback(&PacketTrace));
  Config::ConnectWithoutContext ("/NodeList/*/Ipv6L3Protocol/Tx", MakeCallback(&PacketTrace));

  // ICMPv6 Echo Application: STA1 -> STA2
  uint16_t echoPort = 9;
  Ipv6Address sta2addr = if_r2sta2.GetAddress(1, 1); // STA2 address
  Ipv6Address sta1addr = if_sta1r1.GetAddress(0, 1);

  V6PingHelper pingHelper (sta2addr);
  pingHelper.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  pingHelper.SetAttribute ("Size", UintegerValue (64));
  pingHelper.SetAttribute ("Count", UintegerValue (3));
  ApplicationContainer acPing = pingHelper.Install (stas.Get (0));
  acPing.Start (Seconds (1.0));
  acPing.Stop (Seconds (6.0));

  // At time 2.1s, send an ICMPv6 Redirect from R1 to STA1, indicating to use R2 as next hop for STA2.
  Simulator::Schedule (
    Seconds (2.1),
    &SendIpv6Redirect, 
    routers.Get(0), // R1
    sta1addr,       // STA1 address
    sta2addr,       // STA2 address
    if_r1r2.GetAddress(1,1), // R2's address on R1-R2 link
    1,   // R1's ifindex on STA1-R1
    0    // R1's ifindex on R1-R2
  );

  Simulator::Stop (Seconds (7.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}