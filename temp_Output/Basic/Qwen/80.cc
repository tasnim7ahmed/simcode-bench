#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TbfExample");

class TbfTracer {
public:
  TbfTracer(std::string firstBucketTraceFile, std::string secondBucketTraceFile)
    : m_firstBucketStream(0),
      m_secondBucketStream(0),
      m_firstBucketTraceFile(firstBucketTraceFile),
      m_secondBucketTraceFile(secondBucketTraceFile) {}

  void Install(Ptr<QueueDisc> queueDisc) {
    AsciiTraceHelper ascii;
    m_firstBucketStream = ascii.CreateFileStream(m_firstBucketTraceFile);
    m_secondBucketStream = ascii.CreateFileStream(m_secondBucketTraceFile);

    queueDisc->TraceConnectWithoutContext("TokensInFirstBucket", MakeCallback(&TbfTracer::FirstBucketTrace, this));
    queueDisc->TraceConnectWithoutContext("TokensInSecondBucket", MakeCallback(&TbfTracer::SecondBucketTrace, this));
  }

private:
  void FirstBucketTrace(uint32_t oldVal, uint32_t newVal) {
    *m_firstBucketStream->GetStream() << Simulator::Now().GetSeconds() << " " << newVal << std::endl;
  }

  void SecondBucketTrace(uint32_t oldVal, uint32_t newVal) {
    *m_secondBucketStream->GetStream() << Simulator::Now().GetSeconds() << " " << newVal << std::endl;
  }

  Ptr<OutputStreamWrapper> m_firstBucketStream;
  Ptr<OutputStreamWrapper> m_secondBucketStream;
  std::string m_firstBucketTraceFile;
  std::string m_secondBucketTraceFile;
};

int main(int argc, char* argv[]) {
  double simulationTime = 10.0;
  uint32_t firstBucketSize = 1000;
  uint32_t secondBucketSize = 500;
  uint64_t rate = 1024;
  uint64_t peakRate = 2048;
  std::string firstBucketTraceFile = "first-bucket-trace.txt";
  std::string secondBucketTraceFile = "second-bucket-trace.txt";
  std::string queueStatsFile = "tbf-queue-stats.txt";

  CommandLine cmd(__FILE__);
  cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.AddValue("firstBucketSize", "Size of the first bucket in bytes", firstBucketSize);
  cmd.AddValue("secondBucketSize", "Size of the second bucket in bytes", secondBucketSize);
  cmd.AddValue("rate", "Token arrival rate in bytes per second", rate);
  cmd.AddValue("peakRate", "Peak token rate in bytes per second", peakRate);
  cmd.AddValue("firstBucketTraceFile", "Output trace file for first bucket tokens", firstBucketTraceFile);
  cmd.AddValue("secondBucketTraceFile", "Output trace file for second bucket tokens", secondBucketTraceFile);
  cmd.AddValue("queueStatsFile", "Output file for queue statistics", queueStatsFile);
  cmd.Parse(argc, argv);

  NodeContainer nodes;
  nodes.Create(2);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute("DataRate", DataRateValue(DataRate("10Mbps")));
  pointToPoint.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));

  NetDeviceContainer devices;
  devices = pointToPoint.Install(nodes);

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  TrafficControlHelper tch;
  uint16_t handle = tch.SetRootQueueDisc("ns3::TbfQueueDisc",
                                        "MaxSize", QueueSizeValue(QueueSize("100p")),
                                        "Burst", UintegerValue(firstBucketSize),
                                        "Mtu", UintegerValue(secondBucketSize),
                                        "Rate", DataRateValue(DataRate(rate)),
                                        "PeakRate", DataRateValue(DataRate(peakRate)));
  QueueDiscContainer qdiscs = tch.Install(devices);

  TbfTracer tracer(firstBucketTraceFile, secondBucketTraceFile);
  tracer.Install(qdiscs.Get(0));

  uint16_t port = 9;
  OnOffHelper onoff("ns3::UdpSocketFactory", Address(InetSocketAddress(interfaces.GetAddress(1), port)));
  onoff.SetConstantRate(DataRate(rate * 8 / 10)); // Adjust to account for packet overhead
  ApplicationContainer apps = onoff.Install(nodes.Get(0));
  apps.Start(Seconds(0.0));
  apps.Stop(Seconds(simulationTime));

  PacketSinkHelper sink("ns3::UdpSocketFactory", Address(InetSocketAddress(Ipv4Address::GetAny(), port)));
  apps = sink.Install(nodes.Get(1));
  apps.Start(Seconds(0.0));
  apps.Stop(Seconds(simulationTime));

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  Simulator::Stop(Seconds(simulationTime));
  Simulator::Run();

  monitor->CheckForLostPackets();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
  std::ofstream statsStream(queueStatsFile);
  if (statsStream.is_open()) {
    for (auto flowId : monitor->GetFlowIds()) {
      FlowMonitor::FlowStats stats = monitor->GetFlowStats(flowId);
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flowId);
      statsStream << "Flow ID: " << flowId << "\n";
      statsStream << "Source: " << t.sourceAddress << ":" << t.sourcePort << "\n";
      statsStream << "Destination: " << t.destinationAddress << ":" << t.destinationPort << "\n";
      statsStream << "Packets Dropped: " << stats.packetsDropped.size() << "\n";
      statsStream << "Bytes Dropped: " << stats.bytesDropped.size() << "\n";
      statsStream << "Packets Received: " << stats.rxPackets << "\n";
      statsStream << "Bytes Received: " << stats.rxBytes << "\n\n";
    }
  }

  Simulator::Destroy();
  return 0;
}