#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv6-address-helper.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/udp-echo-helper.h"
#include "ns3/ipv6-packet-info-tag.h"
#include "ns3/packet.h"
#include "ns3/packet-sink-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("Ipv6FragmentationPMTUExample");

void
RxTrace(std::string context, Ptr<const Packet> packet, const Address &address)
{
  NS_LOG_UNCOND("Packet received at: " << Simulator::Now().GetSeconds() << "s, size: " << packet->GetSize());
}

int main(int argc, char *argv[])
{
  LogComponentEnable("Ipv6FragmentationPMTUExample", LOG_LEVEL_INFO);
  LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
  LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

  // Nodes: Src, n0, r1, n1, r2, n2
  NodeContainer srcNode;
  srcNode.Create(1);
  NodeContainer n0;
  n0.Create(1);
  NodeContainer r1;
  r1.Create(1);
  NodeContainer n1;
  n1.Create(1);
  NodeContainer r2;
  r2.Create(1);
  NodeContainer n2;
  n2.Create(1);

  // Link1: Src--[CSMA5000]--n0
  NodeContainer link1Nodes = NodeContainer(srcNode.Get(0), n0.Get(0));
  CsmaHelper csma1;
  csma1.SetChannelAttribute("DataRate", DataRateValue(DataRate("10Gbps")));
  csma1.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));
  csma1.SetDeviceAttribute("Mtu", UintegerValue(5000));
  NetDeviceContainer dev1 = csma1.Install(link1Nodes);

  // Link2: n0--[P2P2000]--r1--[P2P1500]--r2
  // n0 <-> r1
  NodeContainer link2Nodes = NodeContainer(n0.Get(0), r1.Get(0));
  PointToPointHelper p2p2;
  p2p2.SetDeviceAttribute("DataRate", StringValue("5Gbps"));
  p2p2.SetChannelAttribute("Delay", StringValue("2ms"));
  p2p2.SetDeviceAttribute("Mtu", UintegerValue(2000));
  NetDeviceContainer dev2 = p2p2.Install(link2Nodes);

  // r1 <-> n1
  NodeContainer link3Nodes = NodeContainer(r1.Get(0), n1.Get(0));
  PointToPointHelper p2p3;
  p2p3.SetDeviceAttribute("DataRate", StringValue("2Gbps"));
  p2p3.SetChannelAttribute("Delay", StringValue("3ms"));
  p2p3.SetDeviceAttribute("Mtu", UintegerValue(1500));
  NetDeviceContainer dev3 = p2p3.Install(link3Nodes);

  // n1 <-> r2
  NodeContainer link4Nodes = NodeContainer(n1.Get(0), r2.Get(0));
  PointToPointHelper p2p4;
  p2p4.SetDeviceAttribute("DataRate", StringValue("2Gbps"));
  p2p4.SetChannelAttribute("Delay", StringValue("3ms"));
  p2p4.SetDeviceAttribute("Mtu", UintegerValue(1500));
  NetDeviceContainer dev4 = p2p4.Install(link4Nodes);

  // r2 <-> n2
  NodeContainer link5Nodes = NodeContainer(r2.Get(0), n2.Get(0));
  PointToPointHelper p2p5;
  p2p5.SetDeviceAttribute("DataRate", StringValue("1Gbps"));
  p2p5.SetChannelAttribute("Delay", StringValue("4ms"));
  p2p5.SetDeviceAttribute("Mtu", UintegerValue(1500));
  NetDeviceContainer dev5 = p2p5.Install(link5Nodes);

  // Install IPv6
  InternetStackHelper internetv6;
  internetv6.SetIpv4StackInstall(false);
  internetv6.Install(srcNode);
  internetv6.Install(n0);
  internetv6.Install(r1);
  internetv6.Install(n1);
  internetv6.Install(r2);
  internetv6.Install(n2);

  // IPv6 Addressing
  Ipv6AddressHelper ipv6;

  // Link1: 2001:1::/64
  ipv6.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
  Ipv6InterfaceContainer if1 = ipv6.Assign(dev1);
  if1.SetForwarding(0, true);
  if1.SetForwarding(1, true);
  if1.SetDefaultRouteInAllNodes(0);

  // Link2: 2001:2::/64
  ipv6.SetBase(Ipv6Address("2001:2::"), Ipv6Prefix(64));
  Ipv6InterfaceContainer if2 = ipv6.Assign(dev2);
  if2.SetForwarding(0, true);
  if2.SetForwarding(1, true);
  if2.SetDefaultRouteInAllNodes(1);

  // Link3: 2001:3::/64
  ipv6.SetBase(Ipv6Address("2001:3::"), Ipv6Prefix(64));
  Ipv6InterfaceContainer if3 = ipv6.Assign(dev3);
  if3.SetForwarding(0, true);
  if3.SetForwarding(1, true);
  if3.SetDefaultRouteInAllNodes(0);

  // Link4: 2001:4::/64
  ipv6.SetBase(Ipv6Address("2001:4::"), Ipv6Prefix(64));
  Ipv6InterfaceContainer if4 = ipv6.Assign(dev4);
  if4.SetForwarding(0, true);
  if4.SetForwarding(1, true);
  if4.SetDefaultRouteInAllNodes(0);

  // Link5: 2001:5::/64
  ipv6.SetBase(Ipv6Address("2001:5::"), Ipv6Prefix(64));
  Ipv6InterfaceContainer if5 = ipv6.Assign(dev5);
  if5.SetForwarding(0, true);
  if5.SetForwarding(1, true);
  if5.SetDefaultRouteInAllNodes(0);

  // Enable forwarding on routers (n0, r1, n1, r2)
  Ptr<Ipv6> ipv6n0 = n0.Get(0)->GetObject<Ipv6>();
  ipv6n0->SetAttribute("IpForward", BooleanValue(true));
  Ptr<Ipv6> ipv6r1 = r1.Get(0)->GetObject<Ipv6>();
  ipv6r1->SetAttribute("IpForward", BooleanValue(true));
  Ptr<Ipv6> ipv6n1 = n1.Get(0)->GetObject<Ipv6>();
  ipv6n1->SetAttribute("IpForward", BooleanValue(true));
  Ptr<Ipv6> ipv6r2 = r2.Get(0)->GetObject<Ipv6>();
  ipv6r2->SetAttribute("IpForward", BooleanValue(true));

  // Configure static routing/default route on Src to n0
  Ipv6StaticRoutingHelper ipv6RoutingHelper;
  Ptr<Ipv6StaticRouting> srcStaticRouting = ipv6RoutingHelper.GetStaticRouting(srcNode.Get(0)->GetObject<Ipv6>());
  srcStaticRouting->SetDefaultRoute(if1.GetAddress(1, 1), 1);

  // Configure static routing/default route on n2 to r2
  Ptr<Ipv6StaticRouting> n2StaticRouting = ipv6RoutingHelper.GetStaticRouting(n2.Get(0)->GetObject<Ipv6>());
  n2StaticRouting->SetDefaultRoute(if5.GetAddress(0, 1), 1);

  // Setup UDP echo server on n2
  uint16_t echoPort = 9;
  UdpEchoServerHelper echoServer(echoPort);
  ApplicationContainer serverApps = echoServer.Install(n2.Get(0));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(20.0));

  // Setup UDP echo client on Src, target = n2
  UdpEchoClientHelper echoClient(if5.GetAddress(1, 1), echoPort);
  echoClient.SetAttribute("MaxPackets", UintegerValue(1));
  echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  // Send very large packet to force fragmentation (MTU < packet size along path)
  echoClient.SetAttribute("PacketSize", UintegerValue(4000));
  ApplicationContainer clientApps = echoClient.Install(srcNode.Get(0));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(20.0));

  // Tracing
  AsciiTraceHelper ascii;
  Ptr<OutputStreamWrapper> stream = ascii.CreateFileStream("fragmentation-ipv6-PMTU.tr");
  // Hook to all sinks at n2
  dev5.Get(1)->TraceConnectWithoutContext("PhyRxEnd", MakeBoundCallback(&RxTrace));
  dev5.Get(1)->TraceConnectWithoutContext("PhyRxEnd", MakeBoundCallback(&PhyRxEndTrace, stream));

  // Also log at server socket
  Config::Connect("/NodeList/*/ApplicationList/*/$ns3::UdpEchoServer/Rx", 
    MakeBoundCallback(&RxTrace));
  // Enable packet printing
  Packet::EnablePrinting();

  Simulator::Stop(Seconds(20.0));
  Simulator::Run();
  Simulator::Destroy();
  return 0;
}