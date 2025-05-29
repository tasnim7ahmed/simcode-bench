#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("TrafficControlSimulation");

int main (int argc, char *argv[])
{
  bool enablePcap = false;
  bool enableDCTCP = false;
  std::string bottleneckQueueDisc = "fqcodel";
  std::string rttTraceFile = "rtt.txt";
  std::string cwndTraceFile = "cwnd.txt";
  std::string queueTraceFile = "queue.txt";
  std::string dropTraceFile = "drop.txt";
  std::string markTraceFile = "mark.txt";
  std::string throughputTraceFile = "throughput.txt";

  CommandLine cmd;
  cmd.AddValue ("enablePcap", "Enable PCAP tracing", enablePcap);
  cmd.AddValue ("enableDCTCP", "Enable DCTCP", enableDCTCP);
  cmd.AddValue ("bottleneckQueueDisc", "Bottleneck queue disc type (fqcodel, pie)", bottleneckQueueDisc);
  cmd.AddValue ("rttTraceFile", "RTT trace file name", rttTraceFile);
  cmd.AddValue ("cwndTraceFile", "CWND trace file name", cwndTraceFile);
  cmd.AddValue ("queueTraceFile", "Queue trace file name", queueTraceFile);
  cmd.AddValue ("dropTraceFile", "Drop trace file name", dropTraceFile);
  cmd.AddValue ("markTraceFile", "Mark trace file name", markTraceFile);
  cmd.AddValue ("throughputTraceFile", "Throughput trace file name", throughputTraceFile);
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);
  LogComponent::SetDefaultLogLevel(LogLevel::LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create (4);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer devices01 = pointToPoint.Install (nodes.Get (0), nodes.Get (1));
  NetDeviceContainer devices12 = pointToPoint.Install (nodes.Get (1), nodes.Get (2));
  NetDeviceContainer devices23 = pointToPoint.Install (nodes.Get (2), nodes.Get (3));

  TrafficControlHelper tch;
  tch.SetRootQueueDisc ("ns3::FqCoDelQueueDisc");
  QueueDiscContainer queueDiscs = tch.Install (devices12);

  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces01 = address.Assign (devices01);

  address.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces12 = address.Assign (devices12);

  address.SetBase ("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces23 = address.Assign (devices23);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // Ping application
  PingHelper ping (interfaces23.GetAddress (1));
  ping.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  ping.SetAttribute ("Size", UintegerValue (1024));
  ApplicationContainer pingApps = ping.Install (nodes.Get (0));
  pingApps.Start (Seconds (1.0));
  pingApps.Stop (Seconds (10.0));

  // Bulk Send application
  uint16_t port = 50000;
  BulkSendHelper bulkSend ("ns3::TcpSocketFactory", InetSocketAddress (interfaces23.GetAddress (1), port));
  bulkSend.SetAttribute ("MaxBytes", UintegerValue (1000000));
  ApplicationContainer sourceApps = bulkSend.Install (nodes.Get (0));
  sourceApps.Start (Seconds (2.0));
  sourceApps.Stop (Seconds (10.0));

  PacketSinkHelper sink ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), port));
  ApplicationContainer sinkApps = sink.Install (nodes.Get (3));
  sinkApps.Start (Seconds (2.0));
  sinkApps.Stop (Seconds (10.0));

  if (enablePcap)
    {
      pointToPoint.EnablePcapAll ("traffic_control");
    }

  Simulator::Stop (Seconds (11.0));

  Simulator::Run ();

  Simulator::Destroy ();
  return 0;
}