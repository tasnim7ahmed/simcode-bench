// icmpv6-redirect.cc
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/ipv6-routing-table-entry.h"
#include "ns3/ipv6-list-routing-helper.h"
#include "ns3/packet-sink-helper.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("Icmpv6RedirectExample");

static void
PacketTrace(std::string context, Ptr<const Packet> packet)
{
  static std::ofstream out("icmpv6-redirect.tr", std::ios_base::app);
  out << Simulator::Now().GetSeconds() << " " << context << " " << *packet << std::endl;
}

int
main(int argc, char *argv[])
{
  LogComponentEnable("Icmpv6RedirectExample", LOG_LEVEL_INFO);

  // Create nodes: STA1-R1-R2-STA2
  NodeContainer c;
  c.Create(4);
  Ptr<Node> sta1 = c.Get(0);
  Ptr<Node> r1 = c.Get(1);
  Ptr<Node> r2 = c.Get(2);
  Ptr<Node> sta2 = c.Get(3);

  // Create links
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("2ms"));

  // STA1 <-> R1
  NodeContainer n_sta1_r1(sta1, r1);
  NetDeviceContainer d_sta1_r1 = p2p.Install(n_sta1_r1);

  // R1 <-> R2
  NodeContainer n_r1_r2(r1, r2);
  NetDeviceContainer d_r1_r2 = p2p.Install(n_r1_r2);

  // R2 <-> STA2
  NodeContainer n_r2_sta2(r2, sta2);
  NetDeviceContainer d_r2_sta2 = p2p.Install(n_r2_sta2);

  // Install Internet stack
  InternetStackHelper internet;
  Ipv6ListRoutingHelper list;
  Ipv6StaticRoutingHelper staticRouting;
  list.Add(staticRouting, 0);
  internet.SetRoutingHelper(list);
  internet.Install(c);

  // Assign IPv6 addresses
  Ipv6AddressHelper ipv6;

  // STA1-R1
  ipv6.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
  Ipv6InterfaceContainer i_sta1_r1 = ipv6.Assign(d_sta1_r1);
  i_sta1_r1.SetForwarding(1, true);
  i_sta1_r1.SetDefaultRouteInAllNodes(1);

  // R1-R2
  ipv6.SetBase(Ipv6Address("2001:2::"), Ipv6Prefix(64));
  Ipv6InterfaceContainer i_r1_r2 = ipv6.Assign(d_r1_r2);
  i_r1_r2.SetForwarding(0, true);
  i_r1_r2.SetForwarding(1, true);

  // R2-STA2
  ipv6.SetBase(Ipv6Address("2001:3::"), Ipv6Prefix(64));
  Ipv6InterfaceContainer i_r2_sta2 = ipv6.Assign(d_r2_sta2);
  i_r2_sta2.SetForwarding(0, true);
  i_r2_sta2.SetDefaultRouteInAllNodes(0);

  // Now configure routing tables

  // STA1: default route via R1's STA1-facing address
  Ptr<Ipv6StaticRouting> sta1Routing = staticRouting.GetStaticRouting(sta1->GetObject<Ipv6>());
  sta1Routing->SetDefaultRoute(i_sta1_r1.GetAddress(1, 1), 1);

  // STA2: default route via R2's STA2-facing address
  Ptr<Ipv6StaticRouting> sta2Routing = staticRouting.GetStaticRouting(sta2->GetObject<Ipv6>());
  sta2Routing->SetDefaultRoute(i_r2_sta2.GetAddress(0, 1), 1); // 1 is the interface to R2

  // R1: static route to STA2 via R2
  Ptr<Ipv6StaticRouting> r1Routing = staticRouting.GetStaticRouting(r1->GetObject<Ipv6>());
  r1Routing->AddNetworkRouteTo(Ipv6Address("2001:3::"), Ipv6Prefix(64), i_r1_r2.GetAddress(1, 1), 2);

  // R2: static route to STA1 via R1
  Ptr<Ipv6StaticRouting> r2Routing = staticRouting.GetStaticRouting(r2->GetObject<Ipv6>());
  r2Routing->AddNetworkRouteTo(Ipv6Address("2001:1::"), Ipv6Prefix(64), i_r1_r2.GetAddress(0, 1), 1);

  // Install Echo server on STA2
  uint16_t echoPort = 9;
  V6PingHelper echoHelper(i_r2_sta2.GetAddress(1, 1));
  echoHelper.SetAttribute("Interval", TimeValue(Seconds(2.0)));
  echoHelper.SetAttribute("Size", UintegerValue(56));
  echoHelper.SetAttribute("Verbose", BooleanValue(true));
  echoHelper.SetAttribute("Count", UintegerValue(4));
  ApplicationContainer apps1 = echoHelper.Install(sta1);
  apps1.Start(Seconds(2.0));
  apps1.Stop(Seconds(16.0));

  // Enable pcap
  p2p.EnablePcapAll("icmpv6-redirect", false);

  // Tracing (custom: output to .tr file)
  Config::Connect("/NodeList/*/DeviceList/*/$ns3::PointToPointNetDevice/TxQueue/Enqueue",
                  MakeCallback(&PacketTrace));
  Config::Connect("/NodeList/*/DeviceList/*/$ns3::PointToPointNetDevice/MacTx",
                  MakeCallback(&PacketTrace));
  Config::Connect("/NodeList/*/DeviceList/*/$ns3::PointToPointNetDevice/MacRx",
                  MakeCallback(&PacketTrace));
  Config::Connect("/NodeList/*/DeviceList/*/$ns3::PointToPointNetDevice/PhyRxEnd",
                  MakeCallback(&PacketTrace));

  // Schedule ICMPv6 redirect after first Echo request is sent
  // To do this, forcibly send a Redirect from R1 to STA1 for STA2 after ping starts.
  Simulator::Schedule(Seconds(4.0), [=]() {
    Ptr<Ipv6L3Protocol> ipv6 = r1->GetObject<Ipv6L3Protocol>();
    if (ipv6)
    {
      Ipv6Address sta1Addr = i_sta1_r1.GetAddress(0, 1);
      Ipv6Address sta1LinkAddr = i_sta1_r1.GetAddress(0, 0); // Link-local
      Ipv6Address sta2Addr = i_r2_sta2.GetAddress(1, 1);

      // Find interface index from R1 to STA1 and R1 to R2
      uint32_t ifToSta1 = 1; // On R1, interface 1 goes STA1<->R1
      uint32_t ifToR2   = 2; // On R1, interface 2 goes R1<->R2

      // The ICMPv6 Redirection is: for destination sta2Addr, next hop is r2's R1-facing addr
      Icmpv6Redirect icmpv6Redirect;
      icmpv6Redirect.SetTargetAddress(i_r1_r2.GetAddress(1, 1)); // R2's addr facing R1
      icmpv6Redirect.SetDestinationAddress(sta2Addr);
      icmpv6Redirect.SetReserved(0);

      Ptr<Packet> packet = Create<Packet>();
      icmpv6Redirect.Serialize(packet);

      // Use the correct send ICMP function (a bit of a hack, but simulate redirection)
      ipv6->SendIcmpv6Redirect(ifToSta1, sta1Addr, sta2Addr, i_r1_r2.GetAddress(1, 1), packet);

      NS_LOG_INFO("R1: Sent ICMPv6 Redirect to STA1 for STA2 (" << sta2Addr << "), use next hop " << i_r1_r2.GetAddress(1,1));
    }
  });

  // Run simulation
  Simulator::Stop(Seconds(20.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}