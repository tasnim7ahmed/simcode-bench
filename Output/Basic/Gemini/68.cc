#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"
#include "ns3/ping-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("NetworkSimulation");

int main (int argc, char *argv[])
{
  bool enableBql = false;
  std::string bottleneckQueueDisc = "PfifoFast";
  std::string bottleneckBandwidth = "10Mbps";
  std::string bottleneckDelay = "2ms";

  CommandLine cmd;
  cmd.AddValue ("enableBql", "Enable Byte Queue Limits (BQL)", enableBql);
  cmd.AddValue ("bottleneckQueueDisc", "Bottleneck queue disc type: PfifoFast, ARED, CoDel, FqCoDel, PIE", bottleneckQueueDisc);
  cmd.AddValue ("bottleneckBandwidth", "Bottleneck bandwidth", bottleneckBandwidth);
  cmd.AddValue ("bottleneckDelay", "Bottleneck delay", bottleneckDelay);
  cmd.Parse (argc, argv);

  LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
  LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create (3);

  PointToPointHelper pointToPointAccess;
  pointToPointAccess.SetDeviceAttribute ("DataRate", StringValue ("100Mbps"));
  pointToPointAccess.SetChannelAttribute ("Delay", StringValue ("0.1ms"));
  if (enableBql) {
      pointToPointAccess.SetQueueAttribute ("UseBql", BooleanValue (true));
  }

  PointToPointHelper pointToPointBottleneck;
  pointToPointBottleneck.SetDeviceAttribute ("DataRate", StringValue (bottleneckBandwidth));
  pointToPointBottleneck.SetChannelAttribute ("Delay", StringValue (bottleneckDelay));
    if (enableBql) {
      pointToPointBottleneck.SetQueueAttribute ("UseBql", BooleanValue (true));
  }

  NetDeviceContainer accessDevices;
  accessDevices = pointToPointAccess.Install (nodes.Get (0), nodes.Get (1));

  NetDeviceContainer bottleneckDevices;
  bottleneckDevices = pointToPointBottleneck.Install (nodes.Get (1), nodes.Get (2));

  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer accessInterfaces = address.Assign (accessDevices);

  address.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer bottleneckInterfaces = address.Assign (bottleneckDevices);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  TrafficControlHelper bottleneckQueue;
  if (bottleneckQueueDisc == "PfifoFast") {
    bottleneckQueue.SetRootQueueDisc ("ns3::PfifoFastQueueDisc");
  } else if (bottleneckQueueDisc == "ARED") {
    bottleneckQueue.SetRootQueueDisc ("ns3::RedQueueDisc");
  } else if (bottleneckQueueDisc == "CoDel") {
    bottleneckQueue.SetRootQueueDisc ("ns3::CoDelQueueDisc");
  } else if (bottleneckQueueDisc == "FqCoDel") {
    bottleneckQueue.SetRootQueueDisc ("ns3::FqCoDelQueueDisc");
  } else if (bottleneckQueueDisc == "PIE") {
    bottleneckQueue.SetRootQueueDisc ("ns3::PieQueueDisc");
  } else {
      std::cerr << "Invalid queue disc type. Using PfifoFast as default." << std::endl;
      bottleneckQueue.SetRootQueueDisc ("ns3::PfifoFastQueueDisc");
  }

  QueueDiscContainer queueDiscs = bottleneckQueue.Install (bottleneckDevices.Get (1));

  uint16_t port = 9;
  UdpServerHelper server (port);
  ApplicationContainer serverApps = server.Install (nodes.Get (2));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  UdpClientHelper client (bottleneckInterfaces.GetAddress (1), port);
  client.SetAttribute ("MaxPackets", UintegerValue (4294967295));
  client.SetAttribute ("Interval", TimeValue (Time ("0.00001")));
  client.SetAttribute ("PacketSize", UintegerValue (1024));

  ApplicationContainer clientApps = client.Install (nodes.Get (0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  UdpServerHelper server2 (port+1);
  ApplicationContainer serverApps2 = server2.Install (nodes.Get (0));
  serverApps2.Start (Seconds (1.0));
  serverApps2.Stop (Seconds (10.0));

  UdpClientHelper client2 (accessInterfaces.GetAddress (0), port+1);
  client2.SetAttribute ("MaxPackets", UintegerValue (4294967295));
  client2.SetAttribute ("Interval", TimeValue (Time ("0.00001")));
  client2.SetAttribute ("PacketSize", UintegerValue (1024));

  ApplicationContainer clientApps2 = client2.Install (nodes.Get (2));
  clientApps2.Start (Seconds (2.0));
  clientApps2.Stop (Seconds (10.0));

  PingHelper ping (bottleneckInterfaces.GetAddress (1));
  ping.SetAttribute ("Interval", TimeValue (Seconds (0.1)));
  ping.SetAttribute ("Size", UintegerValue (1024));
  ApplicationContainer pingApps = ping.Install (nodes.Get (0));
  pingApps.Start (Seconds (2.0));
  pingApps.Stop (Seconds (10.0));

  FlowMonitorHelper flowMonitor;
  Ptr<FlowMonitor> monitor = flowMonitor.InstallAll ();

  Simulator::Stop (Seconds (10.0));

  // Traces
  Config::SetDefault ("ns3::QueueBase::MaxSize", StringValue ("200p"));
  AsciiTraceHelper ascii;
  pointToPointBottleneck.EnableQueueDiscTraces (ascii.CreateFileStream ("queue-limits.tr"), queueDiscs);
  bottleneckQueue.EnableTracesForQueue ("ns3::Queue<Packet>", "BytesInQueue", "bytes-in-queue.tr", bottleneckDevices.Get (1)->GetQueue());

  Simulator::Run ();

  monitor->CheckForLostPackets ();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowMonitor.GetClassifier ());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats ();
  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
      std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
      std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
      std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
      std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1000000 << " Mbps\n";
  }

  monitor->SerializeToXmlFile("flowmon.xml", true, true);

  Simulator::Destroy ();

  return 0;
}