#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

void experiment (std::string queueDisc)
{
  uint32_t nSenders = 7;
  double simulationTime = 10.0;
  std::string bandwidth = "10Mbps";
  std::string delay = "2ms";
  uint16_t sinkPort = 50000;
  std::string tcpVariant = "TcpNewReno";

  Config::SetDefault ("ns3::TcpL4Protocol::SocketType", StringValue (tcpVariant));

  NodeContainer leftNodes, rightNodes, bottleneckNodes;
  leftNodes.Create (nSenders);
  rightNodes.Create (1);
  bottleneckNodes.Create (2);

  NodeContainer allNodes;
  allNodes.Add (leftNodes);
  allNodes.Add (rightNodes);
  allNodes.Add (bottleneckNodes);

  InternetStackHelper stack;
  stack.Install (allNodes);

  PointToPointHelper bottleneckLink;
  bottleneckLink.SetDeviceAttribute ("DataRate", StringValue (bandwidth));
  bottleneckLink.SetChannelAttribute ("Delay", StringValue (delay));

  PointToPointHelper accessLink;
  accessLink.SetDeviceAttribute ("DataRate", StringValue ("100Mbps"));
  accessLink.SetChannelAttribute ("Delay", StringValue ("1ms"));

  NetDeviceContainer bottleneckDevices = bottleneckLink.Install (bottleneckNodes);

  NetDeviceContainer leftDevices, rightDevices;
  for (uint32_t i = 0; i < nSenders; ++i)
    {
      NetDeviceContainer tempDevices = accessLink.Install (leftNodes.Get (i), bottleneckNodes.Get (0));
      leftDevices.Add (tempDevices.Get (0));
    }
  NetDeviceContainer tempDevices = accessLink.Install (rightNodes.Get (0), bottleneckNodes.Get (1));
  rightDevices.Add (tempDevices.Get (0));

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer leftInterfaces, bottleneckInterfaces;
  for (uint32_t i = 0; i < nSenders; ++i)
    {
      Ipv4InterfaceContainer tempInterfaces = address.Assign (leftDevices.Get (i));
      leftInterfaces.Add (tempInterfaces.Get (0));
    }
  address.Assign (bottleneckDevices);
  bottleneckInterfaces = address.Assign (bottleneckDevices);

  Ipv4InterfaceContainer rightInterfaces = address.Assign (rightDevices.Get (0));

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  TrafficControlHelper tch;
  tch.SetRootQueueDisc ("ns3::" + queueDisc);

  QueueDiscContainer queueDiscs;
  queueDiscs.Add (tch.Install (bottleneckDevices.Get (0)));
  queueDiscs.Add (tch.Install (bottleneckDevices.Get (1)));

  ApplicationContainer sinkApp;
  for (uint32_t i = 0; i < 1; ++i)
    {
      PacketSinkHelper sink ("ns3::TcpSocketFactory", InetSocketAddress (rightInterfaces.GetAddress (0), sinkPort));
      sinkApp = sink.Install (rightNodes.Get (i));
      sinkApp.Start (Seconds (0.0));
      sinkApp.Stop (Seconds (simulationTime + 1));
    }

  ApplicationContainer clientApps;
  for (uint32_t i = 0; i < nSenders; ++i)
    {
      BulkSendHelper source ("ns3::TcpSocketFactory", InetSocketAddress (rightInterfaces.GetAddress (0), sinkPort));
      source.SetAttribute ("MaxBytes", UintegerValue (0));
      clientApps = source.Install (leftNodes.Get (i));
      clientApps.Start (Seconds (1.0));
      clientApps.Stop (Seconds (simulationTime));
    }

  AsciiTraceHelper ascii;
  std::string cwndFileName = "dumbbell-" + queueDisc + "-cwnd.txt";
  AsciiTraceHelper::Default = true;
  bottleneckLink.EnableAsciiAll (cwndFileName);

  Simulator::Schedule (Seconds (0.1), [&, queueDisc] () {
    for (uint32_t i = 0; i < 2; ++i)
      {
        std::string queueSizeFileName = "dumbbell-" + queueDisc + "-qsize-" + std::to_string (i) + ".txt";
        QueueDiscTracer::Enable (queueSizeFileName, queueDiscs.Get (i)->GetObject<QueueDisc> (),
                                 &QueueDisc::GetQueueSize);
      }
  });

  Simulator::Stop (Seconds (simulationTime + 1));
  Simulator::Run ();

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();
  monitor->CheckForLostPackets ();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();
  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
      std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
      std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
      std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
      std::cout << "  Lost Packets:   " << i->second.lostPackets << "\n";
      std::cout << "  Delay Sum:   " << i->second.delaySum << "\n";
      std::cout << "  Jitter Sum:   " << i->second.jitterSum << "\n";
    }

  monitor->SerializeToXmlFile ("dumbbell-" + queueDisc + "-flowmon.xml", true, true);

  Simulator::Destroy ();
}

int main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  experiment ("CobaltQueueDisc");
  experiment ("CoDelQueueDisc");

  return 0;
}