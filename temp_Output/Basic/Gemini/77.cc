#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/ripng-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("RipngExample");

int main (int argc, char *argv[])
{
  bool verbose = false;
  bool tracing = true;

  CommandLine cmd;
  cmd.AddValue ("verbose", "Tell application containers to log if set to true", verbose);
  cmd.AddValue ("tracing", "Enable pcap tracing", tracing);

  cmd.Parse (argc,argv);

  if (verbose)
    {
      LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
      LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);
    }

  Time::SetResolution (Time::NS);
  LogComponentEnable ("RipNg", LOG_LEVEL_ALL);
  LogComponentEnable ("Ipv6RoutingTable", LOG_LEVEL_ALL);
  LogComponentEnable ("Ipv6L3Protocol", LOG_LEVEL_ALL);

  NodeContainer nodes;
  nodes.Create (6);
  NodeContainer src = NodeContainer (nodes.Get (0));
  NodeContainer a = NodeContainer (nodes.Get (1));
  NodeContainer b = NodeContainer (nodes.Get (2));
  NodeContainer c = NodeContainer (nodes.Get (3));
  NodeContainer d = NodeContainer (nodes.Get (4));
  NodeContainer dst = NodeContainer (nodes.Get (5));

  InternetStackHelper internet;
  internet.Install (nodes);

  RipNgHelper ripNgRouting;
  ripNgRouting.SetSplitHorizon (true);

  Ipv6StaticRoutingHelper staticRouting;

  ripNgRouting.ExcludeInterface (nodes.Get (1), 1);
  ripNgRouting.ExcludeInterface (nodes.Get (4), 1);

  internet.SetRoutingHelper (ripNgRouting);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("1ms"));
  p2p.SetQueue("ns3::DropTailQueue", "Mode", StringValue("QUEUE_MODE_BYTES"), "MaxBytes", UintegerValue(100000));

  NetDeviceContainer srcA = p2p.Install (src, a);
  NetDeviceContainer aB = p2p.Install (a, b);
  NetDeviceContainer aC = p2p.Install (a, c);
  NetDeviceContainer bC = p2p.Install (b, c);
  NetDeviceContainer bD = p2p.Install (b, d);
  NetDeviceContainer cD = p2p.Install (c, d);
  p2p.SetDeviceAttribute ("DataRate", StringValue ("1Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("10ms"));
  NetDeviceContainer dDst = p2p.Install (d, dst);

  Ipv6AddressHelper ipv6;
  ipv6.SetBase (Ipv6Address ("2001:1::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer iSrcA = ipv6.Assign (srcA);
  Ipv6InterfaceContainer iAB = ipv6.Assign (aB);
  Ipv6InterfaceContainer iAC = ipv6.Assign (aC);
  Ipv6InterfaceContainer iBC = ipv6.Assign (bC);
  Ipv6InterfaceContainer iBD = ipv6.Assign (bD);
  ipv6.SetBase (Ipv6Address ("2001:2::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer iCD = ipv6.Assign (cD);
  Ipv6InterfaceContainer iDDst = ipv6.Assign (dDst);

  iSrcA.GetAddress (1).SetScope (Ipv6Address::SCOPE_LINK_LOCAL);
  iAB.GetAddress (1).SetScope (Ipv6Address::SCOPE_LINK_LOCAL);
  iAC.GetAddress (1).SetScope (Ipv6Address::SCOPE_LINK_LOCAL);
  iBC.GetAddress (1).SetScope (Ipv6Address::SCOPE_LINK_LOCAL);
  iBD.GetAddress (1).SetScope (Ipv6Address::SCOPE_LINK_LOCAL);
  iCD.GetAddress (1).SetScope (Ipv6Address::SCOPE_LINK_LOCAL);
  iDDst.GetAddress (1).SetScope (Ipv6Address::SCOPE_LINK_LOCAL);

  iSrcA.GetAddress (0).SetScope (Ipv6Address::SCOPE_LINK_LOCAL);
  iAB.GetAddress (0).SetScope (Ipv6Address::SCOPE_LINK_LOCAL);
  iAC.GetAddress (0).SetScope (Ipv6Address::SCOPE_LINK_LOCAL);
  iBC.GetAddress (0).SetScope (Ipv6Address::SCOPE_LINK_LOCAL);
  iBD.GetAddress (0).SetScope (Ipv6Address::SCOPE_LINK_LOCAL);
  iCD.GetAddress (0).SetScope (Ipv6Address::SCOPE_LINK_LOCAL);
  iDDst.GetAddress (0).SetScope (Ipv6Address::SCOPE_LINK_LOCAL);

  iAB.GetAddress (0).SetGlobal (Ipv6Address("2001:1:a::1"));
  iDDst.GetAddress (0).SetGlobal (Ipv6Address("2001:2:d::1"));

  for (uint32_t i = 0; i < nodes.GetN (); i++)
    {
      Ptr<Node> node = nodes.Get (i);
      Ptr<Ipv6StaticRouting> routing = staticRouting.GetOrCreateRouting (node->GetObject<Ipv6> ());
      routing->SetDefaultRoute (Ipv6Address::GetZero (), 0, 1);
    }

  Config::SetDefault ("ns3::RipNg", "Metric", UintegerValue (1));
  Config::Set ("/NodeList/*/DeviceList/*/$ns3::PointToPointNetDevice/LinkDelay", StringValue ("1ms"));

  Ptr<Node> srcNode = src.Get (0);
  Ptr<Node> dstNode = dst.Get (0);

  uint16_t port = 9;
  UdpServerHelper server (port);
  ApplicationContainer apps = server.Install (dst);
  apps.Start (Seconds (1.0));
  apps.Stop (Seconds (45.0));

  UdpClientHelper client (iDDst.GetAddress (0), port);
  client.SetAttribute ("MaxPackets", UintegerValue (1000));
  client.SetAttribute ("Interval", TimeValue (Seconds (0.01)));
  client.SetAttribute ("PacketSize", UintegerValue (1024));
  apps = client.Install (src);
  apps.Start (Seconds (3.0));
  apps.Stop (Seconds (45.0));

  Simulator::Schedule (Seconds (40), [&](){
    p2p.SetDeviceAttribute ("DataRate", StringValue ("0Mbps"));
    p2p.SetChannelAttribute ("Delay", StringValue ("1s"));
    NetDeviceContainer bD_broken = p2p.Install (b, d);
  });
  Simulator::Schedule (Seconds (44), [&](){
    p2p.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
    p2p.SetChannelAttribute ("Delay", StringValue ("1ms"));
    NetDeviceContainer bD_fixed = p2p.Install (b, d);
  });

  if (tracing)
    {
      p2p.EnablePcapAll ("ripng-example");
      AsciiTraceHelper ascii;
      p2p.EnableAsciiAll (ascii.CreateFileStream ("ripng-example.tr"));
    }

  AnimationInterface anim ("ripng-animation.xml");
  anim.SetConstantPosition (nodes.Get (0), 1, 1);
  anim.SetConstantPosition (nodes.Get (1), 10, 1);
  anim.SetConstantPosition (nodes.Get (2), 10, 10);
  anim.SetConstantPosition (nodes.Get (3), 20, 1);
  anim.SetConstantPosition (nodes.Get (4), 20, 10);
  anim.SetConstantPosition (nodes.Get (5), 30, 1);

  Simulator::Stop (Seconds (45.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}