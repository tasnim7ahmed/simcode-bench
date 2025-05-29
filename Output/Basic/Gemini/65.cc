#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/ping-helper.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("TrafficControlSimulation");

int main (int argc, char *argv[])
{
  bool tracing = false;
  bool dctcp = false;

  CommandLine cmd;
  cmd.AddValue ("tracing", "Enable pcap tracing", tracing);
  cmd.AddValue ("dctcp", "Enable DCTCP", dctcp);
  cmd.Parse (argc, argv);

  Config::SetDefault ("ns3::TcpL4Protocol::SocketType", StringValue ("ns3::TcpNewReno"));

  if (dctcp)
    {
      Config::SetDefault ("ns3::TcpL4Protocol::SocketType", StringValue ("ns3::TcpDctcp"));
    }

  NodeContainer nodes;
  nodes.Create (2);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer devices;
  devices = pointToPoint.Install (nodes);

  InternetStackHelper stack;
  stack.Install (nodes);

  TrafficControlHelper tch;
  tch.SetRootQueueDisc ("ns3::FqCoDelQueueDisc");
  QueueDiscContainer queueDiscs = tch.Install (devices);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  uint16_t port = 9;

  BulkSendHelper source ("ns3::TcpSocketFactory",
                         InetSocketAddress (interfaces.GetAddress (1), port));
  source.SetAttribute ("MaxBytes", UintegerValue (1000000));
  ApplicationContainer sourceApps = source.Install (nodes.Get (0));
  sourceApps.Start (Seconds (1.0));
  sourceApps.Stop (Seconds (10.0));

  PacketSinkHelper sink ("ns3::TcpSocketFactory",
                         InetSocketAddress (Ipv4Address::GetAny (), port));
  ApplicationContainer sinkApps = sink.Install (nodes.Get (1));
  sinkApps.Start (Seconds (0.0));
  sinkApps.Stop (Seconds (10.0));

  PingHelper ping (interfaces.GetAddress (1));
  ping.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  ping.SetAttribute ("Size", UintegerValue (1024));
  ApplicationContainer pingApps = ping.Install (nodes.Get (0));
  pingApps.Start (Seconds (2.0));
  pingApps.Stop (Seconds (10.0));

  if (tracing)
    {
      pointToPoint.EnablePcapAll ("traffic-control");
    }

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  Simulator::Stop (Seconds (10.0));
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
      std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024 / 1024  << " Mbps\n";
    }

  Simulator::Destroy ();
  return 0;
}