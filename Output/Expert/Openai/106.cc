#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv6-address-helper.h"
#include "ns3/applications-module.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/ipv6-routing-table-entry.h"
#include "ns3/packet-sink.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("Ipv6FragmentationPmtuExample");

void
TracePacketRx(Ptr<const Packet> packet, Ptr<Ipv6> ipv6, uint32_t iif)
{
  std::ofstream out("fragmentation-ipv6-PMTU.tr", std::ios_base::app);
  out << Simulator::Now().GetSeconds() << " Packet received: " << packet->GetSize() << " bytes" << std::endl;
}

int
main(int argc, char *argv[])
{
  LogComponentEnable("Ipv6FragmentationPmtuExample", LOG_LEVEL_INFO);
  LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
  LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

  // Create nodes: Src, r1, n1, r2, n2
  NodeContainer net1; // Src, r1
  net1.Create(2);

  NodeContainer net2; // r1, n1, r2
  net2.Create(3);

  NodeContainer net3; // r2, n2
  net3.Create(2);

  Ptr<Node> src = net1.Get(0);
  Ptr<Node> r1 = net1.Get(1);
  Ptr<Node> n1 = net2.Get(1); // center node
  Ptr<Node> r2 = net2.Get(2);
  Ptr<Node> n2 = net3.Get(1);

  // Link net1: Src <-> r1 (CSMA, MTU 5000)
  CsmaHelper csma1;
  csma1.SetChannelAttribute("DataRate", DataRateValue(DataRate("1Gbps")));
  csma1.SetChannelAttribute("Delay", TimeValue(MilliSeconds(1)));
  csma1.SetDeviceAttribute("Mtu", UintegerValue(5000));
  NetDeviceContainer ndc1 = csma1.Install(NodeContainer(src, r1));

  // Link net2a: r1 <-> n1 (P2P, MTU 2000)
  PointToPointHelper p2pA;
  p2pA.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
  p2pA.SetChannelAttribute("Delay", StringValue("2ms"));
  p2pA.SetDeviceAttribute("Mtu", UintegerValue(2000));
  NetDeviceContainer ndc2a = p2pA.Install(NodeContainer(r1, n1));

  // Link net2b: n1 <-> r2 (P2P, MTU 2000)
  PointToPointHelper p2pB;
  p2pB.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
  p2pB.SetChannelAttribute("Delay", StringValue("2ms"));
  p2pB.SetDeviceAttribute("Mtu", UintegerValue(2000));
  NetDeviceContainer ndc2b = p2pB.Install(NodeContainer(n1, r2));

  // Link net3: r2 <-> n2 (CSMA, MTU 1500)
  CsmaHelper csma3;
  csma3.SetChannelAttribute("DataRate", DataRateValue(DataRate("100Mbps")));
  csma3.SetChannelAttribute("Delay", TimeValue(MilliSeconds(1)));
  csma3.SetDeviceAttribute("Mtu", UintegerValue(1500));
  NetDeviceContainer ndc3 = csma3.Install(NodeContainer(r2, n2));

  // Install IPv6 stack
  InternetStackHelper inet6;
  inet6.Install(NodeContainer(src, r1, n1, r2, n2));

  Ipv6AddressHelper ipv6;
  ipv6.SetBase(Ipv6Address("2001:0:1::"), Ipv6Prefix(64));
  Ipv6InterfaceContainer iic1 = ipv6.Assign(ndc1);
  iic1.SetForwarding(0, true);
  iic1.SetForwarding(1, true);
  iic1.SetDefaultRouteInAllNodes(0);

  ipv6.SetBase(Ipv6Address("2001:0:2::"), Ipv6Prefix(64));
  Ipv6InterfaceContainer iic2a = ipv6.Assign(ndc2a);
  iic2a.SetForwarding(0, true);
  iic2a.SetForwarding(1, true);

  ipv6.SetBase(Ipv6Address("2001:0:3::"), Ipv6Prefix(64));
  Ipv6InterfaceContainer iic2b = ipv6.Assign(ndc2b);
  iic2b.SetForwarding(0, true);
  iic2b.SetForwarding(1, true);

  ipv6.SetBase(Ipv6Address("2001:0:4::"), Ipv6Prefix(64));
  Ipv6InterfaceContainer iic3 = ipv6.Assign(ndc3);
  iic3.SetForwarding(0, true);
  iic3.SetForwarding(1, true);

  // Enable global forwarding on routers
  Ptr<Ipv6> ipv6Src = src->GetObject<Ipv6>();
  Ptr<Ipv6> ipv6R1 = r1->GetObject<Ipv6>();
  Ptr<Ipv6> ipv6N1 = n1->GetObject<Ipv6>();
  Ptr<Ipv6> ipv6R2 = r2->GetObject<Ipv6>();
  Ptr<Ipv6> ipv6N2 = n2->GetObject<Ipv6>();

  ipv6Src->SetIpForward(true);
  ipv6R1->SetIpForward(true);
  ipv6N1->SetIpForward(true);
  ipv6R2->SetIpForward(true);
  ipv6N2->SetIpForward(true);

  // Routing configuration
  Ptr<Ipv6StaticRouting> srcStaticRouting = Ipv6RoutingHelper::GetRouting<Ipv6StaticRouting>(src->GetObject<Ipv6>()->GetRoutingProtocol());
  Ptr<Ipv6StaticRouting> n2StaticRouting = Ipv6RoutingHelper::GetRouting<Ipv6StaticRouting>(n2->GetObject<Ipv6>()->GetRoutingProtocol());

  srcStaticRouting->SetDefaultRoute(iic1.GetAddress(1, 1), 1);
  n2StaticRouting->SetDefaultRoute(iic3.GetAddress(0, 1), 1);

  Ptr<Ipv6StaticRouting> r1Routing = Ipv6RoutingHelper::GetRouting<Ipv6StaticRouting>(r1->GetObject<Ipv6>()->GetRoutingProtocol());
  r1Routing->AddNetworkRouteTo(Ipv6Address("2001:0:4::"), Ipv6Prefix(64), iic2a.GetAddress(1, 1), 2);

  Ptr<Ipv6StaticRouting> r2Routing = Ipv6RoutingHelper::GetRouting<Ipv6StaticRouting>(r2->GetObject<Ipv6>()->GetRoutingProtocol());
  r2Routing->AddNetworkRouteTo(Ipv6Address("2001:0:1::"), Ipv6Prefix(64), iic2b.GetAddress(0, 1), 1);

  Ptr<Ipv6StaticRouting> n1Routing = Ipv6RoutingHelper::GetRouting<Ipv6StaticRouting>(n1->GetObject<Ipv6>()->GetRoutingProtocol());
  n1Routing->AddNetworkRouteTo(Ipv6Address("2001:0:1::"), Ipv6Prefix(64), iic2a.GetAddress(0, 1), 1);
  n1Routing->AddNetworkRouteTo(Ipv6Address("2001:0:4::"), Ipv6Prefix(64), iic2b.GetAddress(1, 1), 2);

  // UDP echo server on n2
  uint16_t port = 9;
  UdpEchoServerHelper echoServer(port);
  ApplicationContainer serverApps = echoServer.Install(n2);
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  // UDP echo client on src, large payload to initiate fragmentation
  UdpEchoClientHelper echoClient(iic3.GetAddress(1, 1), port);
  echoClient.SetAttribute("MaxPackets", UintegerValue(1));
  echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  echoClient.SetAttribute("PacketSize", UintegerValue(4000)); // larger than all but first MTU

  ApplicationContainer clientApps = echoClient.Install(src);
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(10.0));

  // Tracing: Packet receptions at n2
  Config::ConnectWithoutContext(
    "/NodeList/" + std::to_string(n2->GetId()) +
    "/$ns3::Ipv6L3Protocol/Rx", MakeCallback(&TracePacketRx));

  Simulator::Stop(Seconds(11.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}