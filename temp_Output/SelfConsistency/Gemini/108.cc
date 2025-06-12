#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/ipv6-routing-table-entry.h"
#include "ns3/icmpv6-header.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("Icmpv6RedirectExample");

int main() {
  // Enable logging
  LogComponent::EnableIcmpv6();
  LogComponent::EnableIpv6();
  LogComponent::EnablePacketBuffering();

  // Create nodes
  NodeContainer nodes;
  nodes.Create(4);

  NodeContainer sta1Node = NodeContainer(nodes.Get(0));
  NodeContainer r1Node = NodeContainer(nodes.Get(1));
  NodeContainer r2Node = NodeContainer(nodes.Get(2));
  NodeContainer sta2Node = NodeContainer(nodes.Get(3));

  // Create point-to-point channels
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("1Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("10ms"));

  NetDeviceContainer sta1R1Devs = p2p.Install(sta1Node, r1Node);
  NetDeviceContainer r1R2Devs = p2p.Install(r1Node, r2Node);
  NetDeviceContainer r2Sta2Devs = p2p.Install(r2Node, sta2Node);

  // Install Internet stack
  InternetStackHelper internet;
  internet.Install(nodes);

  // Assign IPv6 addresses
  Ipv6AddressHelper ipv6;
  ipv6.SetBase(Ipv6Address("2001:db8:1::"), Ipv6Prefix(64));
  Ipv6InterfaceContainer sta1R1Iface = ipv6.Assign(sta1R1Devs);

  ipv6.SetBase(Ipv6Address("2001:db8:2::"), Ipv6Prefix(64));
  Ipv6InterfaceContainer r1R2Iface = ipv6.Assign(r1R2Devs);

  ipv6.SetBase(Ipv6Address("2001:db8:3::"), Ipv6Prefix(64));
  Ipv6InterfaceContainer r2Sta2Iface = ipv6.Assign(r2Sta2Devs);

  // Configure routing
  Ipv6StaticRoutingHelper ipv6RoutingHelper;
  Ptr<Ipv6StaticRouting> sta1Routing = ipv6RoutingHelper.GetStaticRouting(sta1Node->GetObject<Ipv6>());
  sta1Routing->AddDefaultRoute(sta1R1Iface.GetAddress(1), 1); // Default route to R1

  Ptr<Ipv6StaticRouting> r1Routing = ipv6RoutingHelper.GetStaticRouting(r1Node->GetObject<Ipv6>());
  r1Routing->AddHostRouteToNetwork(r2Sta2Iface.GetAddress(1), r1R2Iface.GetAddress(1), 1); // Route to STA2 via R2

  Ptr<Ipv6StaticRouting> sta2Routing = ipv6RoutingHelper.GetStaticRouting(sta2Node->GetObject<Ipv6>());
  sta2Routing->AddDefaultRoute(r2Sta2Iface.GetAddress(0), 1); // Default route to R2

  // Create and install an application (ping6)
  V6EchoClientHelper ping6(r2Sta2Iface.GetAddress(1));
  ping6.SetAttribute("Interval", TimeValue(Seconds(1)));
  ping6.SetAttribute("MaxPackets", UintegerValue(1));
  ApplicationContainer apps = ping6.Install(sta1Node);
  apps.Start(Seconds(2));
  apps.Stop(Seconds(5));

  // Enable IPv6 forwarding on routers
  r1Node->GetObject<Ipv6>()->SetForwarding(0, true);
  r2Node->GetObject<Ipv6>()->SetForwarding(0, true);

  // Enable pcap tracing
  p2p.EnablePcapAll("icmpv6-redirect");

  // Run the simulation
  Simulator::Stop(Seconds(10));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}