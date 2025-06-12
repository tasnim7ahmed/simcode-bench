#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("RedQueueDiscExample");

class QueueDiscLogger
{
public:
  QueueDiscLogger(Ptr<QueueDisc> queueDisc, std::string label)
      : m_queueDisc(queueDisc), m_label(label)
  {
  }

  void LogQueue()
  {
    uint32_t qlen = m_queueDisc->GetCurrentSize().GetValue();
    NS_LOG_UNCOND(Simulator::Now().GetSeconds() << "s " << m_label << " Queue size: " << qlen);
    Simulator::Schedule(Seconds(0.1), &QueueDiscLogger::LogQueue, this);
  }

private:
  Ptr<QueueDisc> m_queueDisc;
  std::string m_label;
};

int main(int argc, char *argv[])
{
  CommandLine cmd(__FILE__);
  cmd.Parse(argc, argv);

  Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpNewReno::GetTypeId()));

  NodeContainer nodes;
  nodes.Create(2);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

  // Configure RED queue disc
  TrafficControlHelper tch;
  tch.SetRootQueueDisc("ns3::RedQueueDisc");
  tch.AddInternalQueues("ns3::DropTailQueue", "MaxSize", StringValue("1000p"));

  NetDeviceContainer devices = pointToPoint.Install(nodes.Get(0), nodes.Get(1));
  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  // Install queue disc on the device
  QueueDiscContainer qdiscs = tch.Install(devices);

  // Schedule logging of queue lengths
  QueueDiscLogger logger(qdiscs.Get(0), "RED Queue");
  Simulator::Schedule(Seconds(0.1), &QueueDiscLogger::LogQueue, &logger);

  // Install TCP applications
  uint16_t port = 5000;
  BulkSendHelper source("ns3::TcpSocketFactory", InetSocketAddress(interfaces.GetAddress(1), port));
  source.SetAttribute("MaxBytes", UintegerValue(10000000));
  ApplicationContainer sourceApp = source.Install(nodes.Get(0));
  sourceApp.Start(Seconds(1.0));
  sourceApp.Stop(Seconds(10.0));

  PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
  ApplicationContainer sinkApp = sink.Install(nodes.Get(1));
  sinkApp.Start(Seconds(1.0));
  sinkApp.Stop(Seconds(10.0));

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  Simulator::Stop(Seconds(11.0));
  Simulator::Run();

  // Print flow statistics
  monitor->CheckForLostPackets();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i)
  {
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
    NS_LOG_UNCOND("Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << "):");
    NS_LOG_UNCOND("  Tx Packets: " << i->second.txPackets);
    NS_LOG_UNCOND("  Rx Packets: " << i->second.rxPackets);
    NS_LOG_UNCOND("  Lost Packets: " << i->second.lostPackets);
    NS_LOG_UNCOND("  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1000 / 1000 << " Mbps");
    NS_LOG_UNCOND("  Mean Delay: " << i->second.delaySum.GetSeconds() / i->second.rxPackets << " s");
    NS_LOG_UNCOND("  Jitter: " << i->second.jitterSum.GetSeconds() / (i->second.rxPackets > 1 ? (i->second.rxPackets - 1) : 1) << " s");
  }

  Simulator::Destroy();
  return 0;
}