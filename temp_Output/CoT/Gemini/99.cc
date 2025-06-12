#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/netanim-module.h"
#include "ns3/pcap-file.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("ThreeRouterNetwork");

int main (int argc, char *argv[])
{
  bool tracing = true;
  bool pcapTracing = true;

  CommandLine cmd;
  cmd.AddValue ("tracing", "Enable or disable packet tracing", tracing);
  cmd.AddValue ("pcap", "Enable or disable PCAP tracing", pcapTracing);
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);
  LogComponent::SetFilter ("ThreeRouterNetwork", LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create (3);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer devicesAB = pointToPoint.Install (nodes.Get (0), nodes.Get (1));
  NetDeviceContainer devicesBC = pointToPoint.Install (nodes.Get (1), nodes.Get (2));

  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfacesAB = address.Assign (devicesAB);

  address.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer interfacesBC = address.Assign (devicesBC);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // Set up static routes
  Ptr<Ipv4StaticRouting> staticRoutingA = Ipv4RoutingHelper::GetStaticRouting (nodes.Get (0)->GetObject<Ipv4> ());
  staticRoutingA->AddHostRouteToNetwork (Ipv4Address ("10.1.2.2"), Ipv4Address ("0.0.0.0"), 1);

  Ptr<Ipv4StaticRouting> staticRoutingC = Ipv4RoutingHelper::GetStaticRouting (nodes.Get (2)->GetObject<Ipv4> ());
  staticRoutingC->AddHostRouteToNetwork (Ipv4Address ("10.1.1.1"), Ipv4Address ("0.0.0.0"), 1);

  // Inject routes at node B (Transit Router)
  Ptr<Ipv4StaticRouting> staticRoutingB = Ipv4RoutingHelper::GetStaticRouting (nodes.Get (1)->GetObject<Ipv4> ());
  staticRoutingB->AddHostRouteToNetwork (Ipv4Address ("10.1.2.2"), Ipv4Address ("10.1.2.2"), 1);
  staticRoutingB->AddHostRouteToNetwork (Ipv4Address ("10.1.1.1"), Ipv4Address ("10.1.1.1"), 1);
  
  uint16_t port = 9;
  OnOffHelper onoff ("ns3::UdpSocketFactory",
                     Address (InetSocketAddress (interfacesBC.GetAddress (1), port)));
  onoff.SetConstantRate (DataRate ("1Mbps"));

  ApplicationContainer app = onoff.Install (nodes.Get (0));
  app.Start (Seconds (1.0));
  app.Stop (Seconds (10.0));

  PacketSinkHelper sink ("ns3::UdpSocketFactory",
                         InetSocketAddress (Ipv4Address::GetAny (), port));
  ApplicationContainer sinkApps = sink.Install (nodes.Get (2));
  sinkApps.Start (Seconds (1.0));
  sinkApps.Stop (Seconds (10.0));

  if (tracing == true)
    {
      AsciiTraceHelper ascii;
      pointToPoint.EnableAsciiAll (ascii.CreateFileStream ("three-router-network.tr"));
    }

  if (pcapTracing == true)
    {
      pointToPoint.EnablePcapAll ("three-router-network");
    }

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  Simulator::Stop (Seconds (11.0));

  Simulator::Run ();

  monitor->CheckForLostPackets ();

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats ();

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
      NS_LOG_UNCOND ("Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")");
      NS_LOG_UNCOND ("  Tx Bytes:   " << i->second.txBytes);
      NS_LOG_UNCOND ("  Rx Bytes:   " << i->second.rxBytes);
      NS_LOG_UNCOND ("  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024  << " kbps");
    }

  monitor->SerializeToXmlFile("three-router-network.flowmon", true, true);

  Simulator::Destroy ();

  return 0;
}