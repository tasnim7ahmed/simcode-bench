#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("TrafficControlSimulation");

int main (int argc, char *argv[])
{
  bool enablePcap = false;
  bool enableDCTCP = false;
  std::string bottleneckQueueDisc = "fqcodel";
  double simulationTime = 10.0;
  uint32_t queueSize = 1000;
  std::string bandwidth = "10Mbps";
  std::string delay = "2ms";

  CommandLine cmd;
  cmd.AddValue ("enablePcap", "Enable PCAP tracing", enablePcap);
  cmd.AddValue ("enableDCTCP", "Enable DCTCP", enableDCTCP);
  cmd.AddValue ("bottleneckQueueDisc", "Bottleneck queue disc type", bottleneckQueueDisc);
  cmd.AddValue ("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.AddValue ("queueSize", "Queue size in packets", queueSize);
  cmd.AddValue ("bandwidth", "Bottleneck bandwidth", bandwidth);
  cmd.AddValue ("delay", "Bottleneck delay", delay);
  cmd.Parse (argc, argv);

  Config::SetDefault ("ns3::TcpL4Protocol::SocketType", StringValue ("ns3::TcpNewReno"));
  if (enableDCTCP)
    {
      Config::SetDefault ("ns3::TcpL4Protocol::SocketType", StringValue ("ns3::TcpDctcp"));
    }

  NodeContainer nodes;
  nodes.Create (4);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue (bandwidth));
  pointToPoint.SetChannelAttribute ("Delay", StringValue (delay));
  QueueProperties props;
  props.maxSize = QueueSize (QueueSizeUnit::PACKETS, queueSize);

  TrafficControlHelper tch;
  tch.SetRootQueueDisc ("ns3::FqCoDelQueueDisc", "MaxSize", QueueSizeValue (QueueSize (QueueSizeUnit::PACKETS, queueSize)));
  tch.AddQueueDisc ("ns3::FqCoDelQueueDisc", "MaxSize", QueueSizeValue (QueueSize (QueueSizeUnit::PACKETS, queueSize)));

  NetDeviceContainer devices1, devices2;
  devices1 = pointToPoint.Install (nodes.Get (0), nodes.Get (1));
  devices2 = pointToPoint.Install (nodes.Get (2), nodes.Get (3));

  NetDeviceContainer bottleneckDevices;
  bottleneckDevices = pointToPoint.Install (nodes.Get (1), nodes.Get (2));

  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces1 = address.Assign (devices1);

  address.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces2 = address.Assign (bottleneckDevices);

  address.SetBase ("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces3 = address.Assign (devices2);

  tch.Install (bottleneckDevices);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // Setup BulkSend application from node 0 to node 3
  uint16_t port = 50000;
  BulkSendHelper source ("ns3::TcpSocketFactory", InetSocketAddress (interfaces3.GetAddress (1), port));
  source.SetAttribute ("MaxBytes", UintegerValue (0)); // Send unlimited data
  ApplicationContainer sourceApps = source.Install (nodes.Get (0));
  sourceApps.Start (Seconds (1.0));
  sourceApps.Stop (Seconds (simulationTime - 1.0));

  // Setup PacketSink application on node 3
  PacketSinkHelper sink ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), port));
  ApplicationContainer sinkApps = sink.Install (nodes.Get (3));
  sinkApps.Start (Seconds (0.0));
  sinkApps.Stop (Seconds (simulationTime));


  // Add tracing for queue disc
  if (enablePcap)
    {
      pointToPoint.EnablePcapAll ("traffic_control");
    }

  Simulator::Stop (Seconds (simulationTime));

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  Simulator::Run ();

  monitor->CheckForLostPackets ();

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();
  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
      std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
      std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
      std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
      std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1000000 << " Mbps\n";
    }


  monitor->SerializeToXmlFile("traffic-control-flowmon.xml", true, true);

  Simulator::Destroy ();
  return 0;
}