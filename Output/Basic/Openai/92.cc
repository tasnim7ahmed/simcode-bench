#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int
main(int argc, char *argv[])
{
  Time::SetResolution(Time::NS);
  LogComponentEnable("BulkSendApplication", LOG_LEVEL_INFO);
  LogComponentEnable("PacketSink", LOG_LEVEL_INFO);

  // Nodes: src, rtr1, rtr2, rtrDst, dst
  NodeContainer nodes;
  nodes.Create(5);
  Ptr<Node> src = nodes.Get(0);
  Ptr<Node> rtr1 = nodes.Get(1);
  Ptr<Node> rtr2 = nodes.Get(2);
  Ptr<Node> rtrDst = nodes.Get(3);
  Ptr<Node> dst = nodes.Get(4);

  // Multi-interface host: src node with 2 p2p to rtr1, rtr2
  // src<->rtr1
  NodeContainer nSrcRtr1 = NodeContainer(src, rtr1);
  // src<->rtr2
  NodeContainer nSrcRtr2 = NodeContainer(src, rtr2);

  // rtr1<->rtrDst
  NodeContainer nRtr1RtrDst = NodeContainer(rtr1, rtrDst);
  // rtr2<->rtrDst
  NodeContainer nRtr2RtrDst = NodeContainer(rtr2, rtrDst);

  // rtrDst<->dst
  NodeContainer nRtrDstDst = NodeContainer(rtrDst, dst);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("2ms"));

  // Install devices
  NetDeviceContainer devSrcRtr1 = p2p.Install(nSrcRtr1);
  NetDeviceContainer devSrcRtr2 = p2p.Install(nSrcRtr2);
  NetDeviceContainer devRtr1RtrDst = p2p.Install(nRtr1RtrDst);
  NetDeviceContainer devRtr2RtrDst = p2p.Install(nRtr2RtrDst);
  NetDeviceContainer devRtrDstDst = p2p.Install(nRtrDstDst);

  // Install internet stacks
  InternetStackHelper stack;
  stack.Install(nodes);

  // Assign addresses
  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.0.1.0", "255.255.255.0"); // src<->rtr1
  Ipv4InterfaceContainer ifSrcRtr1 = ipv4.Assign(devSrcRtr1);

  ipv4.SetBase("10.0.2.0", "255.255.255.0"); // src<->rtr2
  Ipv4InterfaceContainer ifSrcRtr2 = ipv4.Assign(devSrcRtr2);

  ipv4.SetBase("10.0.3.0", "255.255.255.0"); // rtr1<->rtrDst
  Ipv4InterfaceContainer ifRtr1RtrDst = ipv4.Assign(devRtr1RtrDst);

  ipv4.SetBase("10.0.4.0", "255.255.255.0"); // rtr2<->rtrDst
  Ipv4InterfaceContainer ifRtr2RtrDst = ipv4.Assign(devRtr2RtrDst);

  ipv4.SetBase("10.0.5.0", "255.255.255.0"); // rtrDst<->dst
  Ipv4InterfaceContainer ifRtrDstDst = ipv4.Assign(devRtrDstDst);

  Ipv4Address dstAddr = ifRtrDstDst.GetAddress(1);

  // Static Routing
  // src node: add 2 static routes to dst
  Ptr<Ipv4StaticRouting> srcStatic = Ipv4RoutingHelper::GetRouting<Ipv4StaticRouting>(src->GetObject<Ipv4>()->GetRoutingProtocol());

  // Route 1: via rtr1
  srcStatic->AddNetworkRouteTo(Ipv4Address("10.0.5.0"), Ipv4Mask("255.255.255.0"),
                               Ipv4Address("10.0.1.2"), 1 /* ifIndex for src->rtr1 */);

  // Route 2: via rtr2
  srcStatic->AddNetworkRouteTo(Ipv4Address("10.0.5.0"), Ipv4Mask("255.255.255.0"),
                               Ipv4Address("10.0.2.2"), 2 /* ifIndex for src->rtr2 */);

  // rtr1: dst route
  Ptr<Ipv4StaticRouting> rtr1Static = Ipv4RoutingHelper::GetRouting<Ipv4StaticRouting>(rtr1->GetObject<Ipv4>()->GetRoutingProtocol());
  rtr1Static->AddNetworkRouteTo(Ipv4Address("10.0.5.0"), Ipv4Mask("255.255.255.0"),
                                Ipv4Address("10.0.3.2"), 2);

  // rtr2: dst route
  Ptr<Ipv4StaticRouting> rtr2Static = Ipv4RoutingHelper::GetRouting<Ipv4StaticRouting>(rtr2->GetObject<Ipv4>()->GetRoutingProtocol());
  rtr2Static->AddNetworkRouteTo(Ipv4Address("10.0.5.0"), Ipv4Mask("255.255.255.0"),
                                Ipv4Address("10.0.4.2"), 2);

  // rtrDst: dst route (local)
  Ptr<Ipv4StaticRouting> rtrDstStatic = Ipv4RoutingHelper::GetRouting<Ipv4StaticRouting>(rtrDst->GetObject<Ipv4>()->GetRoutingProtocol());
  rtrDstStatic->AddNetworkRouteTo(Ipv4Address("10.0.5.0"), Ipv4Mask("255.255.255.0"), 3);

  // rtrDst: routes to src<->rtr1 subnet
  rtrDstStatic->AddNetworkRouteTo(Ipv4Address("10.0.1.0"), Ipv4Mask("255.255.255.0"), Ipv4Address("10.0.3.1"), 1);

  // rtrDst: routes to src<->rtr2 subnet
  rtrDstStatic->AddNetworkRouteTo(Ipv4Address("10.0.2.0"), Ipv4Mask("255.255.255.0"), Ipv4Address("10.0.4.1"), 2);

  // rtr1: route to src
  rtr1Static->AddNetworkRouteTo(Ipv4Address("10.0.2.0"), Ipv4Mask("255.255.255.0"), Ipv4Address("10.0.1.1"), 1);
  rtr1Static->AddNetworkRouteTo(Ipv4Address("10.0.1.0"), Ipv4Mask("255.255.255.0"), 1);

  // rtr2: route to src
  rtr2Static->AddNetworkRouteTo(Ipv4Address("10.0.1.0"), Ipv4Mask("255.255.255.0"), Ipv4Address("10.0.2.1"), 1);
  rtr2Static->AddNetworkRouteTo(Ipv4Address("10.0.2.0"), Ipv4Mask("255.255.255.0"), 1);

  // rtr1: route to rtr2
  rtr1Static->AddNetworkRouteTo(Ipv4Address("10.0.4.0"), Ipv4Mask("255.255.255.0"), Ipv4Address("10.0.3.2"), 2);

  // rtr2: route to rtr1
  rtr2Static->AddNetworkRouteTo(Ipv4Address("10.0.3.0"), Ipv4Mask("255.255.255.0"), Ipv4Address("10.0.4.2"), 2);

  // dst: default route to rtrDst
  Ptr<Ipv4StaticRouting> dstStatic = Ipv4RoutingHelper::GetRouting<Ipv4StaticRouting>(dst->GetObject<Ipv4>()->GetRoutingProtocol());
  dstStatic->SetDefaultRoute(Ipv4Address("10.0.5.1"), 1);

  // Create packet sink on dst
  uint16_t port = 50000;
  Address sinkAddress(InetSocketAddress(dstAddr, port));
  PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", sinkAddress);
  ApplicationContainer sinkApp = sinkHelper.Install(dst);
  sinkApp.Start(Seconds(0.0));
  sinkApp.Stop(Seconds(20.0));

  // Bulk send app on src: Choose different outgoing interface by binding socket
  // First flow: go via src->rtr1
  Ptr<Socket> srcSocket1 = Socket::CreateSocket(src, TcpSocketFactory::GetTypeId());
  srcSocket1->Bind(InetSocketAddress(ifSrcRtr1.GetAddress(0), 0));

  Ptr<BulkSendApplication> bulkSendApp1 = CreateObject<BulkSendApplication>();
  bulkSendApp1->SetAttribute("Remote", AddressValue(sinkAddress));
  bulkSendApp1->SetAttribute("SendSize", UintegerValue(1024));
  bulkSendApp1->SetAttribute("MaxBytes", UintegerValue(1024 * 50));
  bulkSendApp1->SetSocket(srcSocket1);

  src->AddApplication(bulkSendApp1);
  bulkSendApp1->SetStartTime(Seconds(1.0));
  bulkSendApp1->SetStopTime(Seconds(6.0));

  // Second flow: go via src->rtr2
  Ptr<Socket> srcSocket2 = Socket::CreateSocket(src, TcpSocketFactory::GetTypeId());
  srcSocket2->Bind(InetSocketAddress(ifSrcRtr2.GetAddress(0), 0));

  Ptr<BulkSendApplication> bulkSendApp2 = CreateObject<BulkSendApplication>();
  bulkSendApp2->SetAttribute("Remote", AddressValue(sinkAddress));
  bulkSendApp2->SetAttribute("SendSize", UintegerValue(1024));
  bulkSendApp2->SetAttribute("MaxBytes", UintegerValue(1024 * 50));
  bulkSendApp2->SetSocket(srcSocket2);

  src->AddApplication(bulkSendApp2);
  bulkSendApp2->SetStartTime(Seconds(8.0));
  bulkSendApp2->SetStopTime(Seconds(13.0));

  Simulator::Stop(Seconds(20));
  Simulator::Run();
  Simulator::Destroy();
  return 0;
}