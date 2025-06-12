#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"
#include "ns3/queue.h"
#include "ns3/ipv4-global-routing-helper.h"
#include <fstream>
#include <string>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("TcpSeries");

Ptr<Queue> GetQueueObject (std::string queueId)
{
  Ptr<Queue> queue = Names::Find<Queue> (queueId);
  return queue;
}

void EnqueueTrace (std::string queueId, Time enqueuedTime, uint32_t packetSize)
{
  std::ofstream queueTracesFile;
  queueTracesFile.open ("results/queueTraces", std::ios::app);
  queueTracesFile << Simulator::Now ().GetSeconds () << " " << queueId << " ENQUEUE " << enqueuedTime.GetSeconds () << " " << packetSize << std::endl;
  queueTracesFile.close ();
}

void DequeueTrace (std::string queueId, Time dequeuedTime, uint32_t packetSize)
{
  std::ofstream queueTracesFile;
  queueTracesFile.open ("results/queueTraces", std::ios::app);
  queueTracesFile << Simulator::Now ().GetSeconds () << " " << queueId << " DEQUEUE " << dequeuedTime.GetSeconds () << " " << packetSize << std::endl;
  queueTracesFile.close ();
}

void DropTrace (std::string queueId, Time droppedTime, uint32_t packetSize)
{
  std::ofstream queueTracesFile;
  queueTracesFile.open ("results/queueTraces", std::ios::app);
  queueTracesFile << Simulator::Now ().GetSeconds () << " " << queueId << " DROP " << droppedTime.GetSeconds () << " " << packetSize << std::endl;
  queueTracesFile.close ();
}

int main (int argc, char *argv[])
{
  bool tracing = true;
  uint32_t maxBytes = 0;

  CommandLine cmd;
  cmd.AddValue ("tracing", "Enable pcap tracing", tracing);
  cmd.AddValue ("maxBytes", "Maximum bytes allowed on queue", maxBytes);
  cmd.Parse (argc, argv);

  std::ofstream configFile;
  configFile.open ("results/config.txt");
  configFile << "Tracing: " << tracing << std::endl;
  configFile << "MaxBytes: " << maxBytes << std::endl;
  configFile.close ();

  NodeContainer nodes;
  nodes.Create (4);

  PointToPointHelper p2p01;
  p2p01.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  p2p01.SetChannelAttribute ("Delay", StringValue ("1ms"));
  if (maxBytes > 0)
    {
      p2p01.SetQueue ("ns3::DropTailQueue", "MaxBytes", UintegerValue (maxBytes));
    }
  NetDeviceContainer d01 = p2p01.Install (nodes.Get (0), nodes.Get (1));

  PointToPointHelper p2p12;
  p2p12.SetDeviceAttribute ("DataRate", StringValue ("1Mbps"));
  p2p12.SetChannelAttribute ("Delay", StringValue ("10ms"));
  if (maxBytes > 0)
    {
      p2p12.SetQueue ("ns3::DropTailQueue", "MaxBytes", UintegerValue (maxBytes));
    }
  NetDeviceContainer d12 = p2p12.Install (nodes.Get (1), nodes.Get (2));

  PointToPointHelper p2p23;
  p2p23.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  p2p23.SetChannelAttribute ("Delay", StringValue ("1ms"));
  if (maxBytes > 0)
    {
      p2p23.SetQueue ("ns3::DropTailQueue", "MaxBytes", UintegerValue (maxBytes));
    }
  NetDeviceContainer d23 = p2p23.Install (nodes.Get (2), nodes.Get (3));

  InternetStackHelper internet;
  internet.Install (nodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i01 = ipv4.Assign (d01);

  ipv4.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer i12 = ipv4.Assign (d12);

  ipv4.SetBase ("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer i23 = ipv4.Assign (d23);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  uint16_t port = 50000;
  BulkSendHelper source ("ns3::TcpSocketFactory", InetSocketAddress (i23.GetAddress (1), port));
  source.SetAttribute ("SendSize", UintegerValue (1448));
  ApplicationContainer sourceApps = source.Install (nodes.Get (0));
  sourceApps.Start (Seconds (1.0));
  sourceApps.Stop (Seconds (10.0));

  PacketSinkHelper sink ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), port));
  ApplicationContainer sinkApps = sink.Install (nodes.Get (3));
  sinkApps.Start (Seconds (0.0));
  sinkApps.Stop (Seconds (10.0));

  if (tracing)
    {
      p2p01.EnablePcap ("results/pcap/tcp-series-01", d01, false);
      p2p12.EnablePcap ("results/pcap/tcp-series-12", d12, false);
      p2p23.EnablePcap ("results/pcap/tcp-series-23", d23, false);
    }

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  AsciiTraceHelper asciiTraceHelper;
  PointToPointHelper::EnableAsciiAll (asciiTraceHelper.CreateFileStream ("results/queue-size.dat"));

  Simulator::Schedule (Seconds (0.1), [](){
      Ptr<Queue> q1 = GetQueueObject ("/NodeList/1/DeviceList/0/$ns3::PointToPointNetDevice/TxQueue");
      Ptr<Queue> q2 = GetQueueObject ("/NodeList/2/DeviceList/0/$ns3::PointToPointNetDevice/TxQueue");

      if (q1 != NULL)
        {
          q1->TraceConnectWithoutContext ("Enqueue", MakeBoundCallback (&EnqueueTrace, "/NodeList/1/DeviceList/0/$ns3::PointToPointNetDevice/TxQueue"));
          q1->TraceConnectWithoutContext ("Dequeue", MakeBoundCallback (&DequeueTrace, "/NodeList/1/DeviceList/0/$ns3::PointToPointNetDevice/TxQueue"));
          q1->TraceConnectWithoutContext ("Drop", MakeBoundCallback (&DropTrace, "/NodeList/1/DeviceList/0/$ns3::PointToPointNetDevice/TxQueue"));
        }

      if (q2 != NULL)
        {
          q2->TraceConnectWithoutContext ("Enqueue", MakeBoundCallback (&EnqueueTrace, "/NodeList/2/DeviceList/0/$ns3::PointToPointNetDevice/TxQueue"));
          q2->TraceConnectWithoutContext ("Dequeue", MakeBoundCallback (&DequeueTrace, "/NodeList/2/DeviceList/0/$ns3::PointToPointNetDevice/TxQueue"));
          q2->TraceConnectWithoutContext ("Drop", MakeBoundCallback (&DropTrace, "/NodeList/2/DeviceList/0/$ns3::PointToPointNetDevice/TxQueue"));
        }
    });

  Config::ConnectWithoutContext ("/NodeList/0/$ns3::TcpL4Protocol/SocketList/0/CongestionWindow", MakeCallback (&
                                                                                                              {
                                                                                                                  static std::ofstream cWndStream ("results/cwndTraces");
                                                                                                                  cWndStream << Simulator::Now ().GetSeconds () << " " <<  *cwnd << std::endl;
                                                                                                                  cWndStream.flush ();
                                                                                                              }));

  Simulator::Stop (Seconds (10.0));

  Simulator::Run ();

  monitor->CheckForLostPackets ();

  std::ofstream queueStatsFile;
  queueStatsFile.open ("results/queueStats.txt");

  Ptr<Queue> q1 = GetQueueObject ("/NodeList/1/DeviceList/0/$ns3::PointToPointNetDevice/TxQueue");
  Ptr<Queue> q2 = GetQueueObject ("/NodeList/2/DeviceList/0/$ns3::PointToPointNetDevice/TxQueue");

  if (q1 != NULL) {
    queueStatsFile << "Queue 1: " << q1->GetStats() << std::endl;
  }
  if (q2 != NULL) {
    queueStatsFile << "Queue 2: " << q2->GetStats() << std::endl;
  }
  queueStatsFile.close();

  int j=0;
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();
  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
      std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
      std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
      std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
      std::cout << "  Lost Packets:   " << i->second.lostPackets << "\n";
      std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds () - i->second.timeFirstTxPacket.GetSeconds ()) / 1000000  << " Mbps\n";
      j++;
    }

  monitor->SerializeToXmlFile("results/flowmon.xml", true, true);

  Simulator::Destroy ();

  return 0;
}