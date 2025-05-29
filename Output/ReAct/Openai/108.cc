#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-helper.h"
#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("Icmpv6RedirectSimulation");

void SendRedirect (Ptr<Node> r1, Ptr<Node> sta1, Ipv6Address sta1Addr, Ipv6Address sta2Addr, Ipv6Address nextHop)
{
  Ptr<Ipv6L3Protocol> ipv6 = r1->GetObject<Ipv6L3Protocol> ();
  Icmpv6Header icmpv6;
  icmpv6.SetType (Icmpv6Header::ICMPV6_REDIRECT);

  // ICMPv6 Redirect option format: Target Address, Destination Address, Link-Layer Address Option
  // We'll create a payload that matches typical redirect content.
  // For simulation: the key point is to create an ICMPv6 Redirect with correct addresses.

  // Build the option buffer
  uint8_t payload[40] = {0};
  // Target address (16 bytes): Next hop address (R2)
  Ipv6Address::ConvertFrom (nextHop).CopyTo (payload);
  // Destination address (16 bytes): Original destination (STA2)
  Ipv6Address::ConvertFrom (sta2Addr).CopyTo (payload + 16);

  Ptr<Packet> p = Create<Packet> (payload, 32); // 16 bytes for target, 16 bytes for dest
  Icmpv6OptionHeader opt;
  opt.SetType (Icmpv6OptionHeader::ICMPV6_OPTION_TARGET_LL_ADDR);
  // For simplicity, no link-layer address included (since NS-3 doesn't attach these by default)
  icmpv6.SetCode (0);
  p->AddHeader (icmpv6);

  // Send ICMPv6 redirect from R1 to STA1
  // We'll use the r1's corresponding interface connected to STA1
  for (uint32_t i = 0; i < r1->GetNDevices(); ++i)
    {
      Ptr<NetDevice> dev = r1->GetDevice(i);
      if (ipv6->GetAddress(i, 1).GetAddress() == Ipv6Address::GetLoopback()) continue;
      ipv6->Send(p->Copy(), ipv6->GetAddress(i, 1).GetAddress(), sta1Addr, 58, 0); // 58=ICMPv6
      break;
    }
}

void
TracePacket(Ptr<const Packet> pkt)
{
  static std::ofstream out("icmpv6-redirect.tr", std::ios_base::app);
  out << Simulator::Now().GetSeconds() << "s PacketUID: " << pkt->GetUid() << " Size: " << pkt->GetSize() << std::endl;
}

int
main (int argc, char *argv[])
{
  Time::SetResolution (Time::NS);
  LogComponentEnable ("Icmpv6RedirectSimulation", LOG_LEVEL_INFO);

  // Nodes: STA1, R1, R2, STA2
  NodeContainer allNodes;
  allNodes.Create(4);
  Ptr<Node> sta1 = allNodes.Get(0);
  Ptr<Node> r1   = allNodes.Get(1);
  Ptr<Node> r2   = allNodes.Get(2);
  Ptr<Node> sta2 = allNodes.Get(3);

  // Links:
  // STA1 <-> R1 <-> R2 <-> STA2

  NodeContainer nSta1R1(sta1, r1);
  NodeContainer nR1R2(r1, r2);
  NodeContainer nR2Sta2(r2, sta2);

  // Install point-to-point links
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));
  NetDeviceContainer dSta1R1 = p2p.Install(nSta1R1);

  p2p.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("3ms"));
  NetDeviceContainer dR1R2 = p2p.Install(nR1R2);

  p2p.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));
  NetDeviceContainer dR2Sta2 = p2p.Install(nR2Sta2);

  // Install IPv6 stacks
  InternetStackHelper inet6;
  inet6.Install(allNodes);

  // Assign addresses
  Ipv6AddressHelper ipv6;

  // STA1-R1: 2001:1::/64
  ipv6.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
  Ipv6InterfaceContainer ifSta1R1 = ipv6.Assign(dSta1R1);
  ifSta1R1.SetForwarding(0, false);
  ifSta1R1.SetForwarding(1, true);
  ifSta1R1.SetDefaultRouteInAllNodes(0); // For STA1
  ifSta1R1.SetDefaultRouteInAllNodes(1);

  // R1-R2: 2001:2::/64
  ipv6.SetBase(Ipv6Address("2001:2::"), Ipv6Prefix(64));
  Ipv6InterfaceContainer ifR1R2 = ipv6.Assign(dR1R2);
  ifR1R2.SetForwarding(0, true);  // R1
  ifR1R2.SetForwarding(1, true);  // R2

  // R2-STA2: 2001:3::/64
  ipv6.SetBase(Ipv6Address("2001:3::"), Ipv6Prefix(64));
  Ipv6InterfaceContainer ifR2Sta2 = ipv6.Assign(dR2Sta2);
  ifR2Sta2.SetForwarding(0, true);
  ifR2Sta2.SetForwarding(1, false);
  ifR2Sta2.SetDefaultRouteInAllNodes(1); // For STA2

  // Configure static routing
  Ipv6StaticRoutingHelper ipv6RoutingHelper;
  Ptr<Ipv6StaticRouting> sta1Static = ipv6RoutingHelper.GetStaticRouting(sta1->GetObject<Ipv6> ());
  Ptr<Ipv6StaticRouting> sta2Static = ipv6RoutingHelper.GetStaticRouting(sta2->GetObject<Ipv6> ());
  Ptr<Ipv6StaticRouting> r1Static = ipv6RoutingHelper.GetStaticRouting(r1->GetObject<Ipv6> ());
  Ptr<Ipv6StaticRouting> r2Static = ipv6RoutingHelper.GetStaticRouting(r2->GetObject<Ipv6> ());

  // Set default routes for STA1 and STA2
  sta1Static->SetDefaultRoute(ifSta1R1.GetAddress(0,1), 1);
  sta2Static->SetDefaultRoute(ifR2Sta2.GetAddress(1,1), 1);

  // R1: static route to STA2 via R2 over its R1-R2 interface
  r1Static->AddNetworkRouteTo(
      Ipv6Address("2001:3::"), 64, // STA2's network
      ifR1R2.GetAddress(0,1), 1);

  // R2: static route to STA2 network (direct connect)
  r2Static->AddNetworkRouteTo(
      Ipv6Address("2001:3::"), 64,
      ifR2Sta2.GetAddress(0,1), 2);

  // Application: Echo from STA1->STA2
  uint16_t echoPort = 9;
  UdpEchoServerHelper echoServer (echoPort);
  ApplicationContainer serverApps = echoServer.Install (sta2);
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  UdpEchoClientHelper echoClient (ifR2Sta2.GetAddress(1,1), echoPort);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (1));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (48));

  ApplicationContainer clientApps = echoClient.Install (sta1);
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  // Tracing: Hook to device receive
  Config::ConnectWithoutContext("/NodeList/*/DeviceList/*/$ns3::PointToPointNetDevice/MacRx", MakeCallback(&TracePacket));

  // Schedule ICMPv6 Redirect after echo request passes via R1->R2 (say, 3.2s)
  Simulator::Schedule(Seconds(3.2), &SendRedirect,
                      r1, sta1,
                      ifSta1R1.GetAddress(0,1), // dest = STA1
                      ifR2Sta2.GetAddress(1,1), // original dst = STA2
                      ifR1R2.GetAddress(0,1));  // next hop = R2's R1-R2 address

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}