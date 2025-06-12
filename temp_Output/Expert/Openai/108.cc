#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/ipv6-address.h"
#include "ns3/ipv6-interface-container.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include <fstream>

using namespace ns3;

static void ReceiveIcmpv6Redirect (Ptr<Socket> socket, Ptr<Node> node)
{
  Ptr<Packet> packet;
  Address from;
  while ((packet = socket->RecvFrom(from)))
    {
      Inet6SocketAddress addr = Inet6SocketAddress::ConvertFrom(from);
      NS_LOG_UNCOND ("Time " << Simulator::Now ().GetSeconds ()
                    << "s: Node " << node->GetId ()
                    << " received ICMPv6 Redirect from "
                    << addr.GetIpv6 ());
    }
}

int main (int argc, char *argv[])
{
  LogComponentEnable ("Ping6Application", LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create (4); // 0: STA1, 1: R1, 2: R2, 3: STA2

  // Name nodes for clarity
  Ptr<Node> sta1 = nodes.Get(0);
  Ptr<Node> r1   = nodes.Get(1);
  Ptr<Node> r2   = nodes.Get(2);
  Ptr<Node> sta2 = nodes.Get(3);

  // Links: STA1<->R1, R1<->R2, R2<->STA2
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer d_st1r1 = p2p.Install (sta1, r1);
  NetDeviceContainer d_r1r2  = p2p.Install (r1, r2);
  NetDeviceContainer d_r2st2 = p2p.Install (r2, sta2);

  // Install Internet stack
  InternetStackHelper stack;
  stack.Install (nodes);

  // Assign IPv6 addresses
  Ipv6AddressHelper ipv6; Ipv6InterfaceContainer if_st1r1, if_r1r2, if_r2st2;

  ipv6.SetBase (Ipv6Address ("2001:1::"), Ipv6Prefix (64));
  if_st1r1 = ipv6.Assign (d_st1r1);
  if_st1r1.SetRouter (1, true);

  ipv6.SetBase (Ipv6Address ("2001:2::"), Ipv6Prefix (64));
  if_r1r2 = ipv6.Assign (d_r1r2);
  if_r1r2.SetRouter (0, true);
  if_r1r2.SetRouter (1, true);

  ipv6.SetBase (Ipv6Address ("2001:3::"), Ipv6Prefix (64));
  if_r2st2 = ipv6.Assign (d_r2st2);
  if_r2st2.SetRouter (0, true);

  // Enable forwarding on routers
  r1->GetObject<Ipv6> ()->SetAttribute ("IpForward", BooleanValue (true));
  r2->GetObject<Ipv6> ()->SetAttribute ("IpForward", BooleanValue (true));

  // Set default routes and static routes
  Ipv6StaticRoutingHelper ipv6RoutingHelper;
  Ptr<Ipv6StaticRouting> sta1StaticRouting = ipv6RoutingHelper.GetStaticRouting (sta1->GetObject<Ipv6> ());
  Ptr<Ipv6StaticRouting> r1StaticRouting = ipv6RoutingHelper.GetStaticRouting (r1->GetObject<Ipv6> ());
  Ptr<Ipv6StaticRouting> r2StaticRouting = ipv6RoutingHelper.GetStaticRouting (r2->GetObject<Ipv6> ());
  Ptr<Ipv6StaticRouting> sta2StaticRouting = ipv6RoutingHelper.GetStaticRouting (sta2->GetObject<Ipv6> ());

  // STA1 Default Route via R1
  sta1StaticRouting->SetDefaultRoute (if_st1r1.GetAddress (1,1), 1);

  // R1 static route to STA2 via R2
  r1StaticRouting->AddNetworkRouteTo (
      Ipv6Address ("2001:3::2"), 128, // STA2 address
      if_r1r2.GetAddress (1,1), 2);

  // R2 Default route via interface toward STA2
  r2StaticRouting->SetDefaultRoute (if_r2st2.GetAddress (0,1), 1);

  // STA2 Default Route via R2
  sta2StaticRouting->SetDefaultRoute (if_r2st2.GetAddress (0,1), 1);

  // Populate FIBs
  Ipv6GlobalRoutingHelper::PopulateRoutingTables ();

  // Application: STA1 -> STA2 (ping6)
  uint16_t packetSize = 56;
  uint32_t maxPackets = 3;
  Time interPacketInterval = Seconds (1.0);

  V6PingHelper ping6 (if_r2st2.GetAddress (1,1)); // STA2 IPv6 address

  ping6.SetAttribute ("MaxPackets", UintegerValue (1));
  ping6.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  ping6.SetAttribute ("PacketSize", UintegerValue (packetSize));

  ApplicationContainer app = ping6.Install (sta1);
  app.Start (Seconds (2.0));
  app.Stop (Seconds (20.0));

  // Schedule additional pings after redirect handled (simulate 2nd and 3rd packets)
  V6PingHelper ping6_after (if_r2st2.GetAddress (1,1));
  ping6_after.SetAttribute ("MaxPackets", UintegerValue (2));
  ping6_after.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  ping6_after.SetAttribute ("PacketSize", UintegerValue (packetSize));
  ApplicationContainer app2 = ping6_after.Install (sta1);
  app2.Start (Seconds (6.0));
  app2.Stop (Seconds (20.0));

  // Enable ICMPv6 redirect generation on R1
  r1->GetObject<Ipv6> ()->SetAttribute ("SendRedirect", BooleanValue (true));

  // ICMPv6 redirect reception logging
  TypeId icmpv6Tid = TypeId::LookupByName ("ns3::Ipv6RawSocketFactory");
  Ptr<Socket> recvSocket = Socket::CreateSocket (sta1, icmpv6Tid);
  recvSocket->SetRecvCallback (MakeBoundCallback (&ReceiveIcmpv6Redirect, sta1));
  Inet6SocketAddress local = Inet6SocketAddress (if_st1r1.GetAddress (0,1));
  recvSocket->Bind (local);

  // Enable pcap tracing if needed
  p2p.EnablePcapAll ("icmpv6-redirect", false, true);

  // Custom packet capture: log flows
  std::ofstream traceFile;
  traceFile.open ("icmpv6-redirect.tr", std::ios::out);

  Config::ConnectWithoutContext ("/NodeList/*/DeviceList/*/$ns3::PointToPointNetDevice/MacRx", 
      MakeCallback ([&traceFile](Ptr<const Packet> packet) {
        traceFile << "t=" << Simulator::Now().GetSeconds() << "s "
                  << "PacketSize=" << packet->GetSize() << std::endl;
      }));

  Simulator::Stop (Seconds (20.0));
  Simulator::Run ();
  traceFile.close ();
  Simulator::Destroy ();

  return 0;
}