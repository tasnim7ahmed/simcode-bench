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
  TbfTracer(double interval, std::string firstBucketFile, std::string secondBucketFile)
      : m_firstBucketStream(ascii_trace_helper_for_file(firstBucketFile)),
        m_secondBucketStream(ascii_trace_helper_for_file(secondBucketFile)),
        m_interval(interval) {}

  void ScheduleTrace(Time time) {
    Simulator::Schedule(time, &TbfTracer::TraceBuckets, this);
  }

  void TraceBuckets() {
    Ptr<TrafficControlLayer> tc = node->GetObject<TrafficControlLayer>();
    if (tc) {
      Ptr<QueueDisc> qd = tc->GetRootQueueDiscOnDevice(dev);
      if (qd && DynamicCast<TokenBucketFilterQueueDisc>(qd)) {
        Ptr<TokenBucketFilterQueueDisc> tbfQd = DynamicCast<TokenBucketFilterQueueDisc>(qd);
        uint32_t tokensFirstBucket = tbfQd->GetFirstBucketTokens();
        uint32_t tokensSecondBucket = tbfQd->GetSecondBucketTokens();

        *m_firstBucketStream->GetStream() << Simulator::Now().GetSeconds() << " " << tokensFirstBucket << std::endl;
        *m_secondBucketStream->GetStream() << Simulator::Now().GetSeconds() << " " << tokensSecondBucket << std::endl;
      }
    }

    if (Simulator::Now() + Seconds(m_interval) < Simulator::GetMaximumSimulationTime()) {
      Simulator::Schedule(Seconds(m_interval), &TbfTracer::TraceBuckets, this);
    }
  }

private:
  AsciiTraceHelper ascii_trace_helper_for_file(std::string filename) {
    return AsciiTraceHelper();
  }

  Ptr<OutputStreamWrapper> m_firstBucketStream;
  Ptr<OutputStreamWrapper> m_secondBucketStream;
  double m_interval;
};

int main(int argc, char *argv[]) {
  double simulationTime = 10.0;
  uint32_t firstBucketSize = 1000;
  uint32_t secondBucketSize = 500;
  uint32_t rate = 500000; // bits per second
  uint32_t peakRate = 1000000; // bits per second
  std::string firstBucketTraceFile = "first-bucket-trace.txt";
  std::string secondBucketTraceFile = "second-bucket-trace.txt";

  CommandLine cmd(__FILE__);
  cmd.AddValue("simulationTime", "Total simulation time (seconds)", simulationTime);
  cmd.AddValue("firstBucketSize", "Size of the first token bucket (bytes)", firstBucketSize);
  cmd.AddValue("secondBucketSize", "Size of the second token bucket (bytes)", secondBucketSize);
  cmd.AddValue("rate", "Token arrival rate (bits/s)", rate);
  cmd.AddValue("peakRate", "Peak rate (bits/s)", peakRate);
  cmd.AddValue("firstBucketTraceFile", "Output file for first bucket trace", firstBucketTraceFile);
  cmd.AddValue("secondBucketTraceFile", "Output file for second bucket trace", secondBucketTraceFile);
  cmd.Parse(argc, argv);

  NodeContainer nodes;
  nodes.Create(2);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute("DataRate", DataRateValue(DataRate("2Mbps")));
  pointToPoint.SetChannelAttribute("Delay", TimeValue(MilliSeconds(5)));

  NetDeviceContainer devices = pointToPoint.Install(nodes);

  TrafficControlHelper tch;
  tch.SetRootQueueDisc("ns3::TokenBucketFilterQueueDisc",
                       "MaxSize", QueueSizeValue(QueueSize("100p")),
                       "FirstBucketSize", UintegerValue(firstBucketSize),
                       "SecondBucketSize", UintegerValue(secondBucketSize),
                       "Rate", DataRateValue(DataRate(rate)),
                       "PeakRate", DataRateValue(DataRate(peakRate)));
  QueueDiscContainer qdiscs = tch.Install(devices);

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  uint16_t port = 9;
  OnOffHelper onoff("ns3::UdpSocketFactory", InetSocketAddress(interfaces.GetAddress(1), port));
  onoff.SetConstantRate(DataRate(rate));
  ApplicationContainer apps = onoff.Install(nodes.Get(0));
  apps.Start(Seconds(1.0));
  apps.Stop(Seconds(simulationTime - 1.0));

  PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
  apps = sink.Install(nodes.Get(1));
  apps.Start(Seconds(0.0));
  apps.Stop(Seconds(simulationTime));

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  TbfTracer tracer(0.1, firstBucketTraceFile, secondBucketTraceFile);
  tracer.ScheduleTrace(Seconds(0.1));

  Simulator::Stop(Seconds(simulationTime));
  Simulator::Run();

  monitor->CheckForLostPackets();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
    NS_LOG_UNCOND("Flow ID: " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")");
    NS_LOG_UNCOND("  Tx Packets: " << i->second.txPackets);
    NS_LOG_UNCOND("  Rx Packets: " << i->second.rxPackets);
    NS_LOG_UNCOND("  Lost Packets: " << i->second.lostPackets);
    NS_LOG_UNCOND("  Throughput: " << i->second.rxBytes * 8.0 / simulationTime / 1000 << " kbps");
  }

  Simulator::Destroy();
  return 0;
}