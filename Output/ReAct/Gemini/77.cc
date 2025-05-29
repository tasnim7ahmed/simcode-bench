#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/ripng-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv6-static-routing.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("RipngExample");

int main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);
  LogComponent::SetDefaultLogLevel(LOG_LEVEL_ALL);
  LogComponent::SetDefaultLogComponentEnable(true);

  NodeContainer nodes;
  nodes.Create (6);
  NodeContainer srcA = NodeContainer (nodes.Get (0), nodes.Get (1));
  NodeContainer aB = NodeContainer (nodes.Get (1), nodes.Get (2));
  NodeContainer aC = NodeContainer (nodes.Get (1), nodes.Get (3));
  NodeContainer bC = NodeContainer (nodes.Get (2), nodes.Get (3));
  NodeContainer bD = NodeContainer (nodes.Get (2), nodes.Get (4));
  NodeContainer dDst = NodeContainer (nodes.Get (4), nodes.Get (5));
  NodeContainer cD = NodeContainer (nodes.Get (3), nodes.Get (4));

  InternetStackHelper internet;
  internet.Install (nodes);

  RipngHelper ripng;
  ripng.SetSplitHorizon (true);
  ripng.SetMetric (1);
  ripng.ExcludeInterface (nodes.Get (3), 2);
  ripng.ExcludeInterface (nodes.Get (2), 2);
  ripng.SetGarbageTimeout (Seconds(30));

  AddressHelper address;
  address.SetBase (Ipv6Address ("2001:1::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer iSrcA = address.Assign (PointToPointHelper().Install (srcA));
  address.SetBase (Ipv6Address ("2001:2::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer iAB = address.Assign (PointToPointHelper().Install (aB));
  address.SetBase (Ipv6Address ("2001:3::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer iAC = address.Assign (PointToPointHelper().Install (aC));
  address.SetBase (Ipv6Address ("2001:4::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer iBC = address.Assign (PointToPointHelper().Install (bC));
  address.SetBase (Ipv6Address ("2001:5::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer iBD = address.Assign (PointToPointHelper().Install (bD));
  address.SetBase (Ipv6Address ("2001:6::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer iDDst = address.Assign (PointToPointHelper().Install (dDst));
  address.SetBase (Ipv6Address ("2001:7::"), Ipv6Prefix (64));
  PointToPointHelper p2pCd;
  p2pCd.SetQueue("ns3::DropTailQueue", "MaxPackets", UintegerValue (20));
  p2pCd.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  p2pCd.SetChannelAttribute ("Delay", StringValue ("2ms"));
  Ipv6InterfaceContainer iCD = address.Assign (p2pCd.Install (cD));

  ripng.SetInterfaceMetric (iCD.GetAddress (0), 10, nodes.Get (3));

  Ipv6StaticRoutingHelper staticRouting;
  Ptr<Ipv6StaticRouting> routerA = staticRouting.GetStaticRouting (nodes.Get (1)->GetObject<Ipv6> ());
  routerA->SetDefaultRoute (iAB.GetAddress (1), 0);
  Ptr<Ipv6StaticRouting> routerD = staticRouting.GetStaticRouting (nodes.Get (4)->GetObject<Ipv6> ());
  routerD->SetDefaultRoute (iDDst.GetAddress (1), 0);

  ripng.Install (nodes.Get (1));
  ripng.Install (nodes.Get (2));
  ripng.Install (nodes.Get (3));
  ripng.Install (nodes.Get (4));

  Ipv6Address dstAddr = iDDst.GetAddress (1);

  V6PingHelper ping (dstAddr);
  ping.SetAttribute ("Verbose", BooleanValue (true));
  ping.SetAttribute ("Interval", TimeValue (Seconds (1)));
  ApplicationContainer p = ping.Install (nodes.Get (0));
  p.Start (Seconds (3.0));
  p.Stop (Seconds (45.0));

  Simulator::Schedule (Seconds (40.0), [&]() {
    std::cout << "Link between B and D Failing" << std::endl;
    bD.Get (0)->GetObject<NetDevice> (0)->SetAttribute("TxDisabled", BooleanValue (true));
    bD.Get (1)->GetObject<NetDevice> (0)->SetAttribute("TxDisabled", BooleanValue (true));
  });

  Simulator::Schedule (Seconds (44.0), [&]() {
    std::cout << "Link between B and D Recovering" << std::endl;
    bD.Get (0)->GetObject<NetDevice> (0)->SetAttribute("TxDisabled", BooleanValue (false));
    bD.Get (1)->GetObject<NetDevice> (0)->SetAttribute("TxDisabled", BooleanValue (false));
  });

  PointToPointHelper p2p;
  p2p.EnablePcapAll ("ripng-example");

  AsciiTraceHelper ascii;
  p2p.EnableAsciiAll (ascii.CreateFileStream ("ripng-example.tr"));

  Simulator::Stop (Seconds (60.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}