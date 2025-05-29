#include <iostream>
#include <fstream>
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/tcp-header.h"
#include "ns3/uinteger.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("TcpPacingExample");

int main (int argc, char *argv[])
{
  bool enablePacing = true;
  double simulationTime = 10.0;
  std::string bottleneckBandwidth = "5Mbps";
  std::string bottleneckDelay = "20ms";
  uint32_t initialCwnd = 10;

  CommandLine cmd;
  cmd.AddValue ("enablePacing", "Enable TCP pacing", enablePacing);
  cmd.AddValue ("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.AddValue ("bottleneckBandwidth", "Bottleneck bandwidth", bottleneckBandwidth);
  cmd.AddValue ("bottleneckDelay", "Bottleneck delay", bottleneckDelay);
  cmd.AddValue ("initialCwnd", "Initial congestion window", initialCwnd);
  cmd.Parse (argc, argv);

  Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue (1460));
  Config::SetDefault ("ns3::TcpSocket::PacingEnabled", BooleanValue (enablePacing));
  Config::SetDefault ("ns3::TcpSocket::InitialCwnd", UintegerValue (initialCwnd));

  NodeContainer nodes;
  nodes.Create (6);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("100Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("10ms"));

  NetDeviceContainer devices01 = p2p.Install (nodes.Get (0), nodes.Get (1));
  NetDeviceContainer devices12 = p2p.Install (nodes.Get (1), nodes.Get (2));

  p2p.SetDeviceAttribute ("DataRate", StringValue (bottleneckBandwidth));
  p2p.SetChannelAttribute ("Delay", StringValue (bottleneckDelay));
  NetDeviceContainer devices23 = p2p.Install (nodes.Get (2), nodes.Get (3));

  p2p.SetDeviceAttribute ("DataRate", StringValue ("100Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("10ms"));
  NetDeviceContainer devices34 = p2p.Install (nodes.Get (3), nodes.Get (4));
  NetDeviceContainer devices35 = p2p.Install (nodes.Get (3), nodes.Get (5));

  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces01 = address.Assign (devices01);

  address.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces12 = address.Assign (devices12);

  address.SetBase ("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces23 = address.Assign (devices23);

  address.SetBase ("10.1.4.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces34 = address.Assign (devices34);

  address.SetBase ("10.1.5.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces35 = address.Assign (devices35);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  uint16_t port = 50000;
  BulkSendHelper source ("ns3::TcpSocketFactory", InetSocketAddress (interfaces23.GetAddress (1), port));
  source.SetAttribute ("SendSize", UintegerValue (1460));
  source.SetAttribute ("MaxBytes", UintegerValue (0));
  ApplicationContainer sourceApps = source.Install (nodes.Get (1));
  sourceApps.Start (Seconds (1.0));
  sourceApps.Stop (Seconds (simulationTime - 1.0));

  PacketSinkHelper sink ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), port));
  ApplicationContainer sinkApps = sink.Install (nodes.Get (3));
  sinkApps.Start (Seconds (0.0));
  sinkApps.Stop (Seconds (simulationTime));

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  Simulator::Stop (Seconds (simulationTime));

  Simulator::Run ();

  monitor->CheckForLostPackets ();

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
      std::cout << "Flow " << i->first  << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
      std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
      std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
      std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";
      std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1000000  << " Mbps\n";
    }

  monitor->SerializeToXmlFile("tcp-pacing.flowmon", true, true);

  Simulator::Destroy ();
  return 0;
}