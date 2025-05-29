#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/traffic-control-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("DctcpFigure17");

struct FlowStats
{
  double throughput;
  uint64_t rxBytes;
};

int main(int argc, char *argv[])
{
  Time::SetResolution(Time::NS);
  LogComponentEnable("DctcpFigure17", LOG_LEVEL_INFO);

  // Constants
  uint32_t numSendersA = 30; // Group 1 - over 10 Gbps bottleneck
  uint32_t numSendersB = 10; // Group 2 - only over 1 Gbps bottleneck
  uint32_t numSenders = numSendersA + numSendersB; // 40 senders
  uint32_t pktSize = 1448;
  std::string dataRateHostT1 = "40Gbps";
  std::string delayHostT1 = "15us";
  std::string dataRateT1T2 = "10Gbps";
  std::string delayT1T2 = "25us";
  std::string dataRateT2R1 = "1Gbps";
  std::string delayT2R1 = "250us";
  std::string dataRateR1Sink = "10Gbps";
  std::string delayR1Sink = "30us";
  double simStopTime = 5.0;
  double startupWindow = 1.0;
  double convergenceTime = 3.0;
  double throughputMeasureDuration = 1.0;

  // Topology: senders -> T1 -> T2 -> R1 -> sink
  NodeContainer sendersA;
  sendersA.Create(numSendersA);
  NodeContainer sendersB;
  sendersB.Create(numSendersB);
  NodeContainer T1, T2, R1, sink;
  T1.Create(1);
  T2.Create(1);
  R1.Create(1);
  sink.Create(1);

  // Devices and channels containers
  NetDeviceContainer devSendersA, devSendersB, devT1T2, devT2R1, devR1Sink;

  // Install Internet
  InternetStackHelper internet;
  internet.Install(sendersA);
  internet.Install(sendersB);
  internet.Install(T1);
  internet.Install(T2);
  internet.Install(R1);
  internet.Install(sink);

  // Host <-> T1 links (40Gbps, 15us)
  PointToPointHelper p2pHostT1;
  p2pHostT1.SetDeviceAttribute("DataRate", StringValue(dataRateHostT1));
  p2pHostT1.SetChannelAttribute("Delay", StringValue(delayHostT1));
  for (uint32_t i = 0; i < numSendersA; ++i)
    devSendersA.Add(p2pHostT1.Install(sendersA.Get(i), T1.Get(0)));
  for (uint32_t i = 0; i < numSendersB; ++i)
    devSendersB.Add(p2pHostT1.Install(sendersB.Get(i), T1.Get(0)));

  // T1 <-> T2 (10Gbps, 25us), bottleneck #1
  PointToPointHelper p2pT1T2;
  p2pT1T2.SetDeviceAttribute("DataRate", StringValue(dataRateT1T2));
  p2pT1T2.SetChannelAttribute("Delay", StringValue(delayT1T2));
  devT1T2 = p2pT1T2.Install(T1.Get(0), T2.Get(0));

  // T2 <-> R1 (1Gbps, 250us), bottleneck #2
  PointToPointHelper p2pT2R1;
  p2pT2R1.SetDeviceAttribute("DataRate", StringValue(dataRateT2R1));
  p2pT2R1.SetChannelAttribute("Delay", StringValue(delayT2R1));
  devT2R1 = p2pT2R1.Install(T2.Get(0), R1.Get(0));

  // R1 <-> sink (10Gbps, 30us)
  PointToPointHelper p2pR1Sink;
  p2pR1Sink.SetDeviceAttribute("DataRate", StringValue(dataRateR1Sink));
  p2pR1Sink.SetChannelAttribute("Delay", StringValue(delayR1Sink));
  devR1Sink = p2pR1Sink.Install(R1.Get(0), sink.Get(0));

  // Use RED with ECN at bottlenecks
  TrafficControlHelper tch;
  tch.SetRootQueueDisc("ns3::RedQueueDisc",
                       "MinTh", DoubleValue(48 * 1024),  // in bytes
                       "MaxTh", DoubleValue(128 * 1024),
                       "LinkBandwidth", StringValue(dataRateT1T2),
                       "LinkDelay", StringValue(delayT1T2),
                       "MeanPktSize", UintegerValue(pktSize),
                       "QW", DoubleValue(0.002),
                       "Mode", StringValue("QUEUE_MODE_BYTES"),
                       "UseEcn", BooleanValue(true));

  QueueDiscContainer qdiscsT1T2 = tch.Install(devT1T2.Get(0)); // T1->T2
  tch.SetRootQueueDisc("ns3::RedQueueDisc",
                       "MinTh", DoubleValue(24 * 1024),  // Bottleneck #2
                       "MaxTh", DoubleValue(64 * 1024),
                       "LinkBandwidth", StringValue(dataRateT2R1),
                       "LinkDelay", StringValue(delayT2R1),
                       "MeanPktSize", UintegerValue(pktSize),
                       "QW", DoubleValue(0.002),
                       "Mode", StringValue("QUEUE_MODE_BYTES"),
                       "UseEcn", BooleanValue(true));
  QueueDiscContainer qdiscsT2R1 = tch.Install(devT2R1.Get(0)); // T2->R1

  // Assign IP addresses
  Ipv4AddressHelper ipv4;
  int subnet = 1;
  std::vector<Ipv4InterfaceContainer> senderAIfs, senderBIfs;
  // sendersA <-> T1
  for (uint32_t i = 0; i < numSendersA; ++i)
    {
      std::ostringstream subnetStr;
      subnetStr << "10." << subnet++ << ".0.0";
      ipv4.SetBase(subnetStr.str().c_str(), "255.255.255.0");
      senderAIfs.push_back(ipv4.Assign(devSendersA.Get(i * 2), devSendersA.Get(i * 2 + 1)));
    }
  // sendersB <-> T1
  for (uint32_t i = 0; i < numSendersB; ++i)
    {
      std::ostringstream subnetStr;
      subnetStr << "10." << subnet++ << ".0.0";
      ipv4.SetBase(subnetStr.str().c_str(), "255.255.255.0");
      senderBIfs.push_back(ipv4.Assign(devSendersB.Get(i * 2), devSendersB.Get(i * 2 + 1)));
    }
  // T1 <-> T2
  ipv4.SetBase("10.1.0.0", "255.255.255.0");
  Ipv4InterfaceContainer ifT1T2 = ipv4.Assign(devT1T2);
  // T2 <-> R1
  ipv4.SetBase("10.2.0.0", "255.255.255.0");
  Ipv4InterfaceContainer ifT2R1 = ipv4.Assign(devT2R1);
  // R1 <-> sink
  ipv4.SetBase("10.3.0.0", "255.255.255.0");
  Ipv4InterfaceContainer ifR1Sink = ipv4.Assign(devR1Sink);

  // Populate routing tables
  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  // Application: Each sender opens 1 TCP flow to sink
  uint16_t basePort = 50000;
  ApplicationContainer sinkApps;
  Address sinkLocalAddress (InetSocketAddress (Ipv4Address::GetAny (), basePort));
  PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory", sinkLocalAddress);
  sinkApps.Add (sinkHelper.Install(sink.Get(0)));

  sinkApps.Start (Seconds(0.0));
  sinkApps.Stop (Seconds(simStopTime));

  // Setup sender apps (BulkSendApplication)
  ApplicationContainer senderApps;
  std::vector<Ptr<BulkSendApplication>> bulkSenders;
  double dt = startupWindow / numSenders;

  // Group A: 30 senders (over both bottlenecks)
  for (uint32_t i = 0; i < numSendersA; ++i)
    {
      BulkSendHelper sender ("ns3::TcpSocketFactory",
                             InetSocketAddress(ifR1Sink.GetAddress(1), basePort));
      sender.SetAttribute("MaxBytes", UintegerValue(0));
      sender.SetAttribute("SendSize", UintegerValue(pktSize));
      ApplicationContainer app = sender.Install(sendersA.Get(i));
      Ptr<BulkSendApplication> bulk = DynamicCast<BulkSendApplication>(app.Get(0));
      bulkSenders.push_back(bulk);
      double startTime = i * dt;
      app.Start(Seconds(startTime));
      app.Stop(Seconds(simStopTime));
      senderApps.Add(app);
    }
  // Group B: 10 senders (only last bottleneck)
  for (uint32_t i = 0; i < numSendersB; ++i)
    {
      BulkSendHelper sender ("ns3::TcpSocketFactory",
                             InetSocketAddress(ifR1Sink.GetAddress(1), basePort));
      sender.SetAttribute("MaxBytes", UintegerValue(0));
      sender.SetAttribute("SendSize", UintegerValue(pktSize));
      ApplicationContainer app = sender.Install(sendersB.Get(i));
      Ptr<BulkSendApplication> bulk = DynamicCast<BulkSendApplication>(app.Get(0));
      bulkSenders.push_back(bulk);
      double startTime = (numSendersA + i) * dt;
      app.Start(Seconds(startTime));
      app.Stop(Seconds(simStopTime));
      senderApps.Add(app);
    }

  // Enable FlowMonitor to measure throughput
  FlowMonitorHelper flowmonHelper;
  Ptr<FlowMonitor> flowmon = flowmonHelper.InstallAll();

  // Queue statistics
  Ptr<QueueDisc> qT1T2 = qdiscsT1T2.Get(0);
  Ptr<QueueDisc> qT2R1 = qdiscsT2R1.Get(0);

  // Trace variables
  uint32_t queueDropsT1T2 = 0, queueMarksT1T2 = 0;
  uint32_t queueDropsT2R1 = 0, queueMarksT2R1 = 0;

  auto DropT1T2 = [&](Ptr<const QueueDiscItem>){
    queueDropsT1T2++; };
  auto MarkT1T2 = [&](Ptr<const QueueDiscItem>){
    queueMarksT1T2++; };
  auto DropT2R1 = [&](Ptr<const QueueDiscItem>){
    queueDropsT2R1++; };
  auto MarkT2R1 = [&](Ptr<const QueueDiscItem>){
    queueMarksT2R1++; };

  qT1T2->TraceConnectWithoutContext("Drop", MakeCallback(DropT1T2));
  qT1T2->TraceConnectWithoutContext("Mark", MakeCallback(MarkT1T2));
  qT2R1->TraceConnectWithoutContext("Drop", MakeCallback(DropT2R1));
  qT2R1->TraceConnectWithoutContext("Mark", MakeCallback(MarkT2R1));

  Simulator::Stop(Seconds(simStopTime));
  Simulator::Run();

  // Throughput measurements (per flow): we take FlowMonitor statistics between t=3s and t=4s
  double startTime = convergenceTime;
  double stopTime = convergenceTime + throughputMeasureDuration;
  flowmon->CheckForLostPackets();

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmonHelper.GetClassifier());
  std::map<FlowId, FlowMonitor::FlowStats> stats = flowmon->GetFlowStats();

  std::vector<FlowStats> flowStats(numSenders);
  double aggThroughput = 0.0;
  std::vector<double> throughputs;

  uint32_t flowIndex = 0;
  for (auto &flow: stats)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flow.first);
      // Only consider flows from our senders to the sink port (basePort)
      if (t.destinationPort != basePort) continue;
      uint64_t rxBytes = 0;
      double timeSpan = stopTime - startTime;
      for (const auto& interval: flow.second.packetsReceivedInIntervals)
        {
          if (interval.first >= startTime && interval.first < stopTime)
            rxBytes += interval.second;
        }
      // If using an older ns-3 version, fallback to totalRxBytes adjusted for time, else try per-interval
      if (rxBytes == 0) // fallback
        {
          double totalTime = std::min(flow.second.timeLastRxPacket.GetSeconds(), stopTime)
                             - std::max(flow.second.timeFirstRxPacket.GetSeconds(), startTime);
          totalTime = std::max(totalTime, 1e-6); // avoid /0
          rxBytes = flow.second.rxBytes;
          timeSpan = totalTime;
        }

      double throughput = (rxBytes * 8.0) / (timeSpan * 1e6); // Mbps
      flowStats[flowIndex].throughput = throughput;
      flowStats[flowIndex].rxBytes = rxBytes;
      if (rxBytes > 0) throughputs.push_back(throughput);
      aggThroughput += throughput;
      flowIndex++;
      if (flowIndex >= numSenders) break;
    }

  std::cout << "Flow throughput during 3-4s window (Mbps):" << std::endl;
  for (uint32_t i = 0; i < numSenders; ++i)
    {
      std::cout << "Flow " << i+1 << ": " << flowStats[i].throughput << std::endl;
    }
  // Jain's Fairness Index
  double sum = 0.0, sumSq = 0.0;
  for (auto thr : throughputs)
    {
      sum += thr;
      sumSq += thr * thr;
    }
  double fairness = (sum * sum) / (throughputs.size() * sumSq + 1e-6);

  std::cout << "Aggregate throughput (Mbps): " << aggThroughput << std::endl;
  std::cout << "Jain's Fairness Index: " << fairness << std::endl;

  std::cout << "Queue statistics (bottleneck links):" << std::endl;
  std::cout << "T1-T2 RED: Drops=" << queueDropsT1T2 << " ECNMarks=" << queueMarksT1T2 << std::endl;
  std::cout << "T2-R1 RED: Drops=" << queueDropsT2R1 << " ECNMarks=" << queueMarksT2R1 << std::endl;

  Simulator::Destroy();
  return 0;
}