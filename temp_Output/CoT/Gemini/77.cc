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
#include "ns3/mobility-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("RipngExample");

int main (int argc, char *argv[])
{
  bool enablePcap = true;
  bool enableAsciiTrace = true;

  CommandLine cmd (__FILE__);
  cmd.AddValue ("EnablePcap", "Enable/disable pcap tracing", enablePcap);
  cmd.AddValue ("EnableAsciiTrace", "Enable/disable Ascii tracing", enableAsciiTrace);
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);
  LogComponent::SetFilter ( "RipngExample", LOG_LEVEL_INFO );

  NodeContainer nodes;
  nodes.Create (6);

  NodeContainer srcA = NodeContainer (nodes.Get (0), nodes.Get (1));
  NodeContainer aB = NodeContainer (nodes.Get (1), nodes.Get (2));
  NodeContainer aC = NodeContainer (nodes.Get (1), nodes.Get (3));
  NodeContainer bC = NodeContainer (nodes.Get (2), nodes.Get (3));
  NodeContainer bD = NodeContainer (nodes.Get (2), nodes.Get (4));
  NodeContainer dDst = NodeContainer (nodes.Get (4), nodes.Get (5));
  NodeContainer cD = NodeContainer (nodes.Get (3), nodes.Get (4));

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("10ms"));

  NetDeviceContainer devicesSrcA = p2p.Install (srcA);
  NetDeviceContainer devicesAB = p2p.Install (aB);
  NetDeviceContainer devicesAC = p2p.Install (aC);
  NetDeviceContainer devicesBC = p2p.Install (bC);
  NetDeviceContainer devicesBD = p2p.Install (bD);
  NetDeviceContainer devicesDDst = p2p.Install (dDst);

  p2p.SetChannelAttribute ("Delay", StringValue ("20ms"));
  NetDeviceContainer devicesCD = p2p.Install (cD);

  InternetStackHelper internetv6;
  internetv6.Install (nodes);

  Ipv6AddressHelper ipv6;
  ipv6.SetBase (Ipv6Address ("2001:1::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer interfacesSrcA = ipv6.Assign (devicesSrcA);
  ipv6.SetBase (Ipv6Address ("2001:2::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer interfacesAB = ipv6.Assign (devicesAB);
  ipv6.SetBase (Ipv6Address ("2001:3::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer interfacesAC = ipv6.Assign (devicesAC);
  ipv6.SetBase (Ipv6Address ("2001:4::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer interfacesBC = ipv6.Assign (devicesBC);
  ipv6.SetBase (Ipv6Address ("2001:5::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer interfacesBD = ipv6.Assign (devicesBD);
  ipv6.SetBase (Ipv6Address ("2001:6::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer interfacesDDst = ipv6.Assign (devicesDDst);
  ipv6.SetBase (Ipv6Address ("2001:7::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer interfacesCD = ipv6.Assign (devicesCD);

  Ptr<Node> A = nodes.Get (1);
  Ptr<Node> D = nodes.Get (4);

  Ipv6StaticRoutingHelper staticRouting;
  Ptr<Ipv6StaticRouting> AstaticRouting = staticRouting.GetStaticRouting (A->GetObject<Ipv6> ());
  AstaticRouting->AddHostRouteToNetmask (Ipv6Address ("2001:6::2"), Ipv6Prefix (128), Ipv6Address ("2001:2::2"), 1);
  Ptr<Ipv6StaticRouting> DstaticRouting = staticRouting.GetStaticRouting (D->GetObject<Ipv6> ());
  DstaticRouting->AddHostRouteToNetmask (Ipv6Address ("2001:1::1"), Ipv6Prefix (128), Ipv6Address ("2001:5::1"), 1);

  RipNgHelper ripngRouting;

  ripngRouting.SetSplitHorizon (true);

  ripngRouting.ExcludeInterface (A, 1);
  ripngRouting.ExcludeInterface (D, 1);

  Address sinkLocalAddress (Inet6SocketAddress (Ipv6Address::GetAny (), 80));
  PacketSinkHelper sinkHelper ("ns3::UdpSocketFactory", sinkLocalAddress);
  ApplicationContainer sinkApp = sinkHelper.Install (nodes.Get (5));
  sinkApp.Start (Seconds (3.0));

  uint16_t port = 80;
  OnOffHelper onOffHelper ("ns3::UdpSocketFactory", Ipv6Address::ConvertFrom(interfacesDDst.GetAddress (1,0)));
  onOffHelper.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
  onOffHelper.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
  onOffHelper.SetAttribute ("PacketSize", UintegerValue (1024));
  onOffHelper.SetAttribute ("DataRate", StringValue ("1Mbps"));
  onOffHelper.SetAttribute ("Remote", AddressValue (Inet6SocketAddress (interfacesDDst.GetAddress (1,0), port)));
  ApplicationContainer clientApps = onOffHelper.Install (nodes.Get (0));
  clientApps.Start (Seconds (3.0));
  clientApps.Stop (Seconds (45.0));

  if (enablePcap)
    {
      p2p.EnablePcapAll ("ripng-example");
    }

  if (enableAsciiTrace)
    {
       AsciiTraceHelper ascii;
       p2p.EnableAsciiAll (ascii.CreateFileStream ("ripng-example.tr"));
    }

  Simulator::Schedule (Seconds (40), [&](){
    devicesBD.Get (0)->GetObject<PointToPointNetDevice> ()->GetChannel ()->Dispose ();
    devicesBD.Get (1)->GetObject<PointToPointNetDevice> ()->GetChannel ()->Dispose ();
  });

  Simulator::Stop (Seconds (45));

  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}