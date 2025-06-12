#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/ripng-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("RipngExample");

int main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);
  LogComponent::SetLogLevel (RipngHelper::GetTypeId (), LOG_LEVEL_ALL);
  LogComponent::SetCategory ("RipngExample");

  // Create nodes
  NodeContainer nodes;
  nodes.Create (6);
  NodeContainer src = NodeContainer (nodes.Get (0));
  NodeContainer a   = NodeContainer (nodes.Get (1));
  NodeContainer b   = NodeContainer (nodes.Get (2));
  NodeContainer c   = NodeContainer (nodes.Get (3));
  NodeContainer d   = NodeContainer (nodes.Get (4));
  NodeContainer dst = NodeContainer (nodes.Get (5));

  // Create channels and set cost
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("10ms"));

  NetDeviceContainer srcA = p2p.Install (src, a);
  NetDeviceContainer aB   = p2p.Install (a, b);
  NetDeviceContainer aC   = p2p.Install (a, c);
  NetDeviceContainer bC   = p2p.Install (b, c);
  NetDeviceContainer bD   = p2p.Install (b, d);
  NetDeviceContainer cD   = p2p.Install (c, d);
  NetDeviceContainer dDst = p2p.Install (d, dst);

  // Install Internet stack
  InternetStackHelper internet;
  internet.Install (nodes);

  // Assign IPv6 Addresses
  Ipv6AddressHelper ipv6;
  ipv6.SetBase (Ipv6Address ("2001:db8::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer iSrcA = ipv6.Assign (srcA);
  Ipv6InterfaceContainer iAB   = ipv6.Assign (aB);
  Ipv6InterfaceContainer iAC   = ipv6.Assign (aC);
  Ipv6InterfaceContainer iBC   = ipv6.Assign (bC);
  Ipv6InterfaceContainer iBD   = ipv6.Assign (bD);
  Ipv6InterfaceContainer iCD   = ipv6.Assign (cD);
  Ipv6InterfaceContainer iDDst = ipv6.Assign (dDst);

  // Set static addresses for A and D
  Ptr<Node> nodeA = a.Get (0);
  Ptr<Node> nodeD = d.Get (0);
  Ptr<Ipv6> ipv6A = nodeA->GetObject<Ipv6> ();
  Ptr<Ipv6> ipv6D = nodeD->GetObject<Ipv6> ();
  int32_t interfaceA1 = ipv6A->GetInterfaceForDevice (srcA.Get (1));
  int32_t interfaceA2 = ipv6A->GetInterfaceForDevice (aB.Get (0));
  int32_t interfaceA3 = ipv6A->GetInterfaceForDevice (aC.Get (0));
  int32_t interfaceD1 = ipv6D->GetInterfaceForDevice (bD.Get (1));
  int32_t interfaceD2 = ipv6D->GetInterfaceForDevice (cD.Get (1));
  int32_t interfaceD3 = ipv6D->GetInterfaceForDevice (dDst.Get (0));

  ipv6A->SetAddress (interfaceA1, Ipv6Address ("2001:db8::1"), Ipv6Prefix (64));
  ipv6A->SetAddress (interfaceA2, Ipv6Address ("2001:db8:1::1"), Ipv6Prefix (64));
  ipv6A->SetAddress (interfaceA3, Ipv6Address ("2001:db8:2::1"), Ipv6Prefix (64));
  ipv6D->SetAddress (interfaceD1, Ipv6Address ("2001:db8:3::2"), Ipv6Prefix (64));
  ipv6D->SetAddress (interfaceD2, Ipv6Address ("2001:db8:4::2"), Ipv6Prefix (64));
  ipv6D->SetAddress (interfaceD3, Ipv6Address ("2001:db8:5::1"), Ipv6Prefix (64));

  // Set RIPng
  RipngHelper ripng;
  ripng.Set ("SplitHorizon", EnumValue (RipngHelper::SPLIT_HORIZON));
  ripng.Install (nodes);

  // Set link cost
  Ptr<Node> nodeC = c.Get (0);
  Ptr<Ipv6StaticRouting> staticRoutingC = Ipv6RoutingHelper::GetRouting <Ipv6StaticRouting> (nodeC->GetObject<Ipv6> ()->GetRoutingProtocol ());
  staticRoutingC->SetLinkMetric (iCD.GetInterface (0), 10);

  // Populate routing tables
  Simulator::Schedule (Seconds (3.0), &RipngHelper::PrintRoutingTableAllAt, &ripng, Seconds (3.0));

  // Create sink application on the destination
  uint16_t port = 9;
  Address sinkLocalAddress (Inet6SocketAddress (Ipv6Address::GetAny (), port));
  PacketSinkHelper sinkHelper ("ns3::UdpSocketFactory", sinkLocalAddress);
  ApplicationContainer sinkApp = sinkHelper.Install (dst);
  sinkApp.Start (Seconds (3.0));
  sinkApp.Stop (Seconds (60.0));

  // Create OnOff application on the source
  OnOffHelper onOffHelper ("ns3::UdpSocketFactory", Address (Inet6SocketAddress (iDDst.GetAddress (1, 0), port)));
  onOffHelper.SetConstantRate (DataRate ("100kbps"));
  ApplicationContainer onOffApp = onOffHelper.Install (src);
  onOffApp.Start (Seconds (3.0));
  onOffApp.Stop (Seconds (60.0));

  // Set error model on link B-D
  Ptr<RateErrorModel> em = CreateObject<RateErrorModel> ();
  em->SetAttribute ("ErrorRate", DoubleValue (0.99));
  bD.Get (0)->SetAttribute ("ReceiveErrorModel", PointerValue (em));

  // Schedule link failure and recovery
  Simulator::Schedule (Seconds (40.0), &RateErrorModel::SetAttribute, em, "ErrorRate", DoubleValue (1.0));
  Simulator::Schedule (Seconds (44.0), &RateErrorModel::SetAttribute, em, "ErrorRate", DoubleValue (0.0));

  // Add PCAP tracing
  p2p.EnablePcapAll ("ripng-example");

  // Add ASCII tracing
  AsciiTraceHelper ascii;
  p2p.EnableAsciiAll (ascii.CreateFileStream ("ripng-example.tr"));

  // Animation Interface
  AnimationInterface anim ("ripng-example.xml");
  anim.SetConstantPosition (nodes.Get (0), 0.0, 2.0);
  anim.SetConstantPosition (nodes.Get (1), 2.0, 2.0);
  anim.SetConstantPosition (nodes.Get (2), 4.0, 2.0);
  anim.SetConstantPosition (nodes.Get (3), 4.0, 0.0);
  anim.SetConstantPosition (nodes.Get (4), 6.0, 1.0);
  anim.SetConstantPosition (nodes.Get (5), 8.0, 1.0);

  Simulator::Stop (Seconds (60));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}