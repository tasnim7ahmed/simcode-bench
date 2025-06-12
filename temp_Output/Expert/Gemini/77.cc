#include <iostream>
#include <fstream>
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/ipv6-routing-table-entry.h"
#include "ns3/ripng.h"
#include "ns3/ripng-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {

  CommandLine cmd;
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);
  LogComponentEnable("RipNg", LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create(6);
  NodeContainer src = NodeContainer(nodes.Get(0));
  NodeContainer a = NodeContainer(nodes.Get(1));
  NodeContainer b = NodeContainer(nodes.Get(2));
  NodeContainer c = NodeContainer(nodes.Get(3));
  NodeContainer d = NodeContainer(nodes.Get(4));
  NodeContainer dst = NodeContainer(nodes.Get(5));

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("1Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("10ms"));

  NetDeviceContainer srcA = p2p.Install(src, a);
  NetDeviceContainer aB = p2p.Install(a, b);
  NetDeviceContainer aC = p2p.Install(a, c);
  NetDeviceContainer bC = p2p.Install(b, c);
  NetDeviceContainer bD = p2p.Install(b, d);
  NetDeviceContainer cD = p2p.Install(c, d);
  p2p.SetChannelAttribute("Cost", UintegerValue(10));
  NetDeviceContainer dDst = p2p.Install(d, dst);
  p2p.SetChannelAttribute("Cost", UintegerValue(1));

  InternetStackHelper internetv6;
  internetv6.Install(nodes);

  Ipv6AddressHelper ipv6;
  ipv6.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
  Ipv6InterfaceContainer iSrcA = ipv6.Assign(srcA);
  Ipv6InterfaceContainer iAB = ipv6.Assign(aB);
  Ipv6InterfaceContainer iAC = ipv6.Assign(aC);
  Ipv6InterfaceContainer iBC = ipv6.Assign(bC);
  Ipv6InterfaceContainer iBD = ipv6.Assign(bD);
  Ipv6InterfaceContainer iCD = ipv6.Assign(cD);
  Ipv6InterfaceContainer iDDst = ipv6.Assign(dDst);

  iSrcA.GetAddress(0, 0);
  iSrcA.GetAddress(1, 0);
  iAB.GetAddress(0, 0);
  iAB.GetAddress(1, 0);
  iAC.GetAddress(0, 0);
  iAC.GetAddress(1, 0);
  iBC.GetAddress(0, 0);
  iBC.GetAddress(1, 0);
  iBD.GetAddress(0, 0);
  iBD.GetAddress(1, 0);
  iCD.GetAddress(0, 0);
  iCD.GetAddress(1, 0);
  iDDst.GetAddress(0, 0);
  iDDst.GetAddress(1, 0);

  iSrcA.GetAddress(0, 0);
  iSrcA.GetAddress(1, 0);
  iAB.GetAddress(0, 0);
  iAB.GetAddress(1, 0);
  iAC.GetAddress(0, 0);
  iAC.GetAddress(1, 0);
  iBC.GetAddress(0, 0);
  iBC.GetAddress(1, 0);
  iBD.GetAddress(0, 0);
  iBD.GetAddress(1, 0);
  iCD.GetAddress(0, 0);
  iCD.GetAddress(1, 0);
  iDDst.GetAddress(0, 0);
  iDDst.GetAddress(1, 0);

  Ptr<Ipv6StaticRouting> staticRoutingA = Ipv6RoutingHelper::GetRouting <Ipv6StaticRouting> (a.Get (0)->GetObject<Ipv6>()->GetRoutingProtocol ());
  staticRoutingA->SetDefaultRoute (iAB.GetAddress (1, 0), 0);

  Ptr<Ipv6StaticRouting> staticRoutingD = Ipv6RoutingHelper::GetRouting <Ipv6StaticRouting> (d.Get (0)->GetObject<Ipv6>()->GetRoutingProtocol ());
  staticRoutingD->SetDefaultRoute (iDDst.GetAddress (1, 0), 0);

  RipNgHelper ripNgRouting;
  ripNgRouting.SetSplitHorizon(true);
  ripNgRouting.ExcludeInterface(a.Get(0), iSrcA.GetInterfaceIndex(0));
  ripNgRouting.ExcludeInterface(dst.Get(0), iDDst.GetInterfaceIndex(0));

  Address sinkLocalAddress(Inet6SocketAddress(Ipv6Address::GetAny(), 80));
    PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", sinkLocalAddress);
    ApplicationContainer sinkApp = sinkHelper.Install(dst.Get(0));
    sinkApp.Start(Seconds(2.0));
    sinkApp.Stop(Seconds(45.0));

  ApplicationContainer apps;
  uint32_t packetSize = 1024;
  uint32_t maxPackets = 1000;
  Time interPacketInterval = Seconds(0.01);
  AddressValue remoteAddress(Inet6SocketAddress(iDDst.GetAddress(1, 0), 80));

  OnOffHelper onoff1("ns3::TcpSocketFactory", remoteAddress);
  onoff1.SetAttribute("PacketSize", UintegerValue(packetSize));
  onoff1.SetAttribute("MaxBytes", UintegerValue(packetSize * maxPackets));
  onoff1.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
  onoff1.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
  apps.Add(onoff1.Install(src.Get(0)));
  apps.Start(Seconds(3.0));
  apps.Stop(Seconds(45.0));


  Simulator::Schedule(Seconds(40.0), [&]() {
    p2p.SetChannelAttribute("Delay", StringValue("1000s"));
    bD.Get(0)->GetChannel()->SetAttribute("Delay", StringValue("1000s"));
    bD.Get(1)->GetChannel()->SetAttribute("Delay", StringValue("1000s"));

  });

  Simulator::Schedule(Seconds(44.0), [&]() {
    p2p.SetChannelAttribute("Delay", StringValue("10ms"));
    bD.Get(0)->GetChannel()->SetAttribute("Delay", StringValue("10ms"));
    bD.Get(1)->GetChannel()->SetAttribute("Delay", StringValue("10ms"));
  });

  p2p.EnablePcapAll("ripng-routing");

  AsciiTraceHelper ascii;
  p2p.EnableAsciiAll(ascii.CreateFileStream("ripng-routing.tr"));

  AnimationInterface anim("ripng-routing.xml");
  anim.SetConstantPosition(nodes.Get(0), 0.0, 10.0);
  anim.SetConstantPosition(nodes.Get(1), 10.0, 10.0);
  anim.SetConstantPosition(nodes.Get(2), 20.0, 10.0);
  anim.SetConstantPosition(nodes.Get(3), 10.0, 0.0);
  anim.SetConstantPosition(nodes.Get(4), 20.0, 0.0);
  anim.SetConstantPosition(nodes.Get(5), 30.0, 0.0);

  Simulator::Stop(Seconds(45.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}