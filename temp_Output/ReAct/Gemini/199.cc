#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("TcpLanPointToPoint");

int main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);
  LogComponent::SetAttribute ("UdpClient", "Interval", StringValue ("0.01"));

  NodeContainer routers;
  routers.Create (3);

  NodeContainer n0_r1, r1_r2, r2_r3, r2_lan;
  n0_r1.Create (1);
  n0_r1.Add (routers.Get (0));
  r1_r2.Add (routers.Get (0));
  r1_r2.Add (routers.Get (1));
  r2_r3.Add (routers.Get (1));
  r2_r3.Add (routers.Get (2));
  r2_lan.Add (routers.Get (1));

  NodeContainer lanNodes;
  lanNodes.Create (2);
  r2_lan.Add (lanNodes.Get (0));
  NodeContainer sinkNode;
  sinkNode.Create (1);
  r2_r3.Add (sinkNode.Get (0));

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer dev_n0_r1 = pointToPoint.Install (n0_r1);
  NetDeviceContainer dev_r1_r2 = pointToPoint.Install (r1_r2);
  NetDeviceContainer dev_r2_r3 = pointToPoint.Install (r2_r3);

  PointToPointHelper pointToPointLan;
  pointToPointLan.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  pointToPointLan.SetChannelAttribute ("Delay", StringValue ("1ms"));

  NetDeviceContainer dev_r2_lan = pointToPointLan.Install (r2_lan);
  NetDeviceContainer dev_r2_r3_sink = pointToPoint.Install (r2_r3);
  NetDeviceContainer dev_r2_r3_source;
  dev_r2_r3_source.Add (dev_r2_r3.Get (0));
  dev_r2_r3_source.Add (dev_r2_r3_sink.Get (1));

  InternetStackHelper stack;
  stack.Install (routers);
  stack.Install (lanNodes);
  stack.Install (n0_r1.Get (0));
  stack.Install (sinkNode.Get (0));

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer if_n0_r1 = address.Assign (dev_n0_r1);

  address.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer if_r1_r2 = address.Assign (dev_r1_r2);

  address.SetBase ("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer if_r2_r3 = address.Assign (dev_r2_r3);

  address.SetBase ("10.1.4.0", "255.255.255.0");
  Ipv4InterfaceContainer if_r2_lan = address.Assign (dev_r2_lan);

  address.SetBase ("10.1.5.0", "255.255.255.0");
  Ipv4InterfaceContainer if_r2_r3_sink = address.Assign (dev_r2_r3_sink);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  uint16_t port = 50000;
  Address sinkAddress (InetSocketAddress (if_r2_r3_sink.GetAddress (1,0), port));
  PacketSinkHelper packetSinkHelper ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), port));
  ApplicationContainer sinkApps = packetSinkHelper.Install (sinkNode.Get (0));
  sinkApps.Start (Seconds (1.0));
  sinkApps.Stop (Seconds (10.0));

  TcpClientHelper tcpClientHelper (if_r2_r3_sink.GetAddress (1,0), port);
  tcpClientHelper.SetAttribute ("MaxPackets", UintegerValue (1000000));
  tcpClientHelper.SetAttribute ("Interval", TimeValue (Seconds (0.01)));
  tcpClientHelper.SetAttribute ("PacketSize", UintegerValue (1024));
  ApplicationContainer clientApps = tcpClientHelper.Install (lanNodes.Get (0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  FlowMonitorHelper flowMonitor;
  Ptr<FlowMonitor> monitor = flowMonitor.InstallAll ();

  Simulator::Stop (Seconds (11.0));

  AnimationInterface anim ("tcp-lan-point-to-point.xml");
  anim.SetConstantPosition (n0_r1.Get (0), 10, 10);
  anim.SetConstantPosition (routers.Get (0), 30, 10);
  anim.SetConstantPosition (routers.Get (1), 50, 10);
  anim.SetConstantPosition (routers.Get (2), 70, 10);
  anim.SetConstantPosition (lanNodes.Get (0), 50, 30);
  anim.SetConstantPosition (lanNodes.Get (1), 50, 50);
  anim.SetConstantPosition (sinkNode.Get (0), 90, 10);

  Simulator::Run ();

  monitor->CheckForLostPackets ();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowMonitor.GetClassifier ());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats ();

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
	  Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
      std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
      std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
      std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
      std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";
      std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1000000  << " Mbps\n";
    }

  monitor->SerializeToXmlFile("tcp-lan-point-to-point.flowmon", true, true);

  Simulator::Destroy ();
  return 0;
}