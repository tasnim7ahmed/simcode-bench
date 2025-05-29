#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/flow-monitor-module.h"
#include <fstream>
#include <iomanip>
#include <vector>
#include <numeric>
#include <algorithm>
#include <cmath>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("DctcpFigure17Simulation");

// Constants derived from paper / description
static const uint32_t N_SENDERS_1 = 30; // 30 senders -> T1
static const uint32_t N_SENDERS_2 = 20; // 20 senders -> T2
static const uint32_t N_FLOWS = 40;
static const double   SIM_TIME = 5.0; // Total: convergence 3s + 1s measure + margin
static const double   STARTUP_WINDOW = 1.0;
static const double   CONVERGENCE_TIME = 3.0;
static const double   MEASURE_START = 3.0;
static const double   MEASURE_END = 4.0;

// Helper to collect throughput and fairness
struct FlowStats
{
  double throughput;
};

int main(int argc, char *argv[])
{
  // Increase maximum open files for many FD usage (optional)
  // Set up output log file
  std::ofstream throughputLog("throughput.txt");
  std::ofstream fairnessLog("fairness.txt");
  std::ofstream queueLog("queue.txt");

  // Enable checksum for robustness
  GlobalValue::Bind("ChecksumEnabled", BooleanValue(true));

  // Create nodes
  NodeContainer senders1; // 30 senders
  senders1.Create(N_SENDERS_1);
  NodeContainer senders2; // 20 senders (makes 40 flows total)
  senders2.Create(N_SENDERS_2);

  Ptr<Node> t1 = CreateObject<Node>(); // First "Top" Switch
  Ptr<Node> t2 = CreateObject<Node>(); // Second "Top" Switch
  Ptr<Node> r1 = CreateObject<Node>(); // Receiver Switch / Rack
  Ptr<Node> receiver = CreateObject<Node>(); // Single receiver

  NodeContainer t1n, t2n, r1n, receiverN;
  t1n.Add(t1);
  t2n.Add(t2);
  r1n.Add(r1);
  receiverN.Add(receiver);

  // Internet stack
  InternetStackHelper internet;
  internet.Install(senders1);
  internet.Install(senders2);
  internet.Install(t1n);
  internet.Install(t2n);
  internet.Install(r1n);
  internet.Install(receiverN);

  // Point-to-Point helpers
  PointToPointHelper p2pHostToT1, p2pHostToT2, p2pT1ToT2, p2pT2ToR1, p2pR1ToRcv;

  p2pHostToT1.SetDeviceAttribute("DataRate", StringValue("10Gbps"));
  p2pHostToT1.SetChannelAttribute("Delay", StringValue("5us"));

  p2pHostToT2.SetDeviceAttribute("DataRate", StringValue("10Gbps"));
  p2pHostToT2.SetChannelAttribute("Delay", StringValue("5us"));

  p2pT1ToT2.SetDeviceAttribute("DataRate", StringValue("10Gbps"));
  p2pT1ToT2.SetChannelAttribute("Delay", StringValue("10us"));

  p2pT2ToR1.SetDeviceAttribute("DataRate", StringValue("1Gbps"));
  p2pT2ToR1.SetChannelAttribute("Delay", StringValue("10us"));

  p2pR1ToRcv.SetDeviceAttribute("DataRate", StringValue("10Gbps"));
  p2pR1ToRcv.SetChannelAttribute("Delay", StringValue("10us"));

  // Traffic Control for RED/ECN on bottlenecks
  TrafficControlHelper tchRed;
  tchRed.SetRootQueueDisc("ns3::RedQueueDisc",
                          "MinTh", DoubleValue(10 * 1024), // 10 packets
                          "MaxTh", DoubleValue(30 * 1024), // 30 packets
                          "LinkBandwidth", StringValue("10Gbps"),
                          "LinkDelay", StringValue("10us"),
                          "QueueLimit", UintegerValue(1000),
                          "UseEcn", BooleanValue(true));

  TrafficControlHelper tchRed2;
  tchRed2.SetRootQueueDisc("ns3::RedQueueDisc",
                          "MinTh", DoubleValue(10 * 1024),
                          "MaxTh", DoubleValue(30 * 1024),
                          "LinkBandwidth", StringValue("1Gbps"),
                          "LinkDelay", StringValue("10us"),
                          "QueueLimit", UintegerValue(1000),
                          "UseEcn", BooleanValue(true));

  // Helper for address assignment
  Ipv4AddressHelper address;

  // Maps for interface assignment
  std::vector<NetDeviceContainer> sender1Devs, sender2Devs;
  NetDeviceContainer t1t2Devs, t2r1Devs, r1recvDevs;

  std::vector<Ipv4InterfaceContainer> sender1Ifs, sender2Ifs;
  Ipv4InterfaceContainer t1t2Ifs, t2r1Ifs, r1recvIfs;

  // Connect senders1 to T1
  for (uint32_t i = 0; i < N_SENDERS_1; ++i)
  {
    NetDeviceContainer link = p2pHostToT1.Install(NodeContainer(senders1.Get(i), t1));
    sender1Devs.push_back(link);
    address.SetBase("10.0." + std::to_string(i) + ".0", "255.255.255.0");
    sender1Ifs.push_back(address.Assign(link));
  }

  // Connect senders2 to T2
  for (uint32_t i = 0; i < N_SENDERS_2; ++i)
  {
    NetDeviceContainer link = p2pHostToT2.Install(NodeContainer(senders2.Get(i), t2));
    sender2Devs.push_back(link);
    address.SetBase("10.1." + std::to_string(i) + ".0", "255.255.255.0");
    sender2Ifs.push_back(address.Assign(link));
  }

  // T1 <-> T2, add RED, ECN marking
  t1t2Devs = p2pT1ToT2.Install(NodeContainer(t1, t2));
  tchRed.Install(t1t2Devs.Get(0));
  tchRed.Install(t1t2Devs.Get(1));
  address.SetBase("11.0.0.0", "255.255.255.0");
  t1t2Ifs = address.Assign(t1t2Devs);

  // T2 <-> R1, bottleneck link with RED-ECN (1Gbps)
  t2r1Devs = p2pT2ToR1.Install(NodeContainer(t2, r1));
  tchRed2.Install(t2r1Devs.Get(0));
  tchRed2.Install(t2r1Devs.Get(1));
  address.SetBase("12.0.0.0", "255.255.255.0");
  t2r1Ifs = address.Assign(t2r1Devs);

  // R1 <-> Receiver
  r1recvDevs = p2pR1ToRcv.Install(NodeContainer(r1, receiver));
  address.SetBase("13.0.0.0", "255.255.255.0");
  r1recvIfs = address.Assign(r1recvDevs);

  // Assign Internet addresses
  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  // Install TCP application on each sender: BulkSend (each to receiver)
  uint16_t port = 10000;
  ApplicationContainer senderApps, sinkApps;

  Address receiverAddr(InetSocketAddress(r1recvIfs.GetAddress(1), port));
  // Install sink application at receiver
  PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
  sinkApps.Add(sinkHelper.Install(receiver));

  // Create 40 flows: 30 from senders1, 10 from senders2 (as per bottleneck description)
  std::vector<Ptr<Application>> bulkApps;
  for (uint32_t i = 0; i < N_SENDERS_1; ++i)
  {
    BulkSendHelper bulkSender("ns3::TcpSocketFactory", receiverAddr);
    bulkSender.SetAttribute("MaxBytes", UintegerValue(0));
    ApplicationContainer app = bulkSender.Install(senders1.Get(i));
    bulkApps.push_back(app.Get(0));
    senderApps.Add(app);
  }
  for (uint32_t i = 0; i < N_SENDERS_2; ++i)
  {
    BulkSendHelper bulkSender("ns3::TcpSocketFactory", receiverAddr);
    bulkSender.SetAttribute("MaxBytes", UintegerValue(0));
    ApplicationContainer app = bulkSender.Install(senders2.Get(i));
    bulkApps.push_back(app.Get(0));
    senderApps.Add(app);
  }

  // Stagger start times during one second, uniform random
  Ptr<UniformRandomVariable> startVar = CreateObject<UniformRandomVariable>();
  for (uint32_t i = 0; i < N_FLOWS; ++i)
  {
    double start = startVar->GetValue(0.0, STARTUP_WINDOW);
    bulkApps[i]->SetStartTime(Seconds(start));
    bulkApps[i]->SetStopTime(Seconds(SIM_TIME));
  }
  sinkApps.Start(Seconds(0.0));
  sinkApps.Stop(Seconds(SIM_TIME));

  // Enable ECN for all TCP sockets -- by default NS-3 TCP doesn't by itself, set at node level
  Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TypeId::LookupByName("ns3::TcpDctcp")));

  // Enable DCTCP for all
  Config::SetDefault("ns3::TcpSocketBase::EnableEcn", BooleanValue(true));

  // FlowMonitor for per-flow throughput/fairness
  FlowMonitorHelper flowmonHelper;
  Ptr<FlowMonitor> flowmon = flowmonHelper.InstallAll();

  // Queue Disc stats: for RED on bottleneck links, record average, max, min queue length
  Ptr<QueueDisc> t1OutRed = StaticCast<QueueDisc>(t1->GetObject<TrafficControlLayer>()->GetRootQueueDiscOnDevice(t1t2Devs.Get(0)));
  Ptr<QueueDisc> t2InRed  = StaticCast<QueueDisc>(t2->GetObject<TrafficControlLayer>()->GetRootQueueDiscOnDevice(t1t2Devs.Get(1)));
  Ptr<QueueDisc> t2OutRed = StaticCast<QueueDisc>(t2->GetObject<TrafficControlLayer>()->GetRootQueueDiscOnDevice(t2r1Devs.Get(0)));
  Ptr<QueueDisc> r1InRed  = StaticCast<QueueDisc>(r1->GetObject<TrafficControlLayer>()->GetRootQueueDiscOnDevice(t2r1Devs.Get(1)));

  // Schedule queue stats collection
  double statsSampleInt = 0.005;
  std::vector<uint32_t> t1OutSamples, t2InSamples, t2OutSamples, r1InSamples;
  std::vector<uint64_t> t1OutEcnMarks, t2InEcnMarks, t2OutEcnMarks, r1InEcnMarks;

  auto QueueStatSample = [&](){
      if (Simulator::Now().GetSeconds() >= MEASURE_START && Simulator::Now().GetSeconds() < MEASURE_END)
      {
        t1OutSamples.push_back(t1OutRed->GetCurrentSize().GetValue());
        t2InSamples.push_back(t2InRed->GetCurrentSize().GetValue());
        t2OutSamples.push_back(t2OutRed->GetCurrentSize().GetValue());
        r1InSamples.push_back(r1InRed->GetCurrentSize().GetValue());

        t1OutEcnMarks.push_back(t1OutRed->GetTotalMarkPkts());
        t2InEcnMarks.push_back(t2InRed->GetTotalMarkPkts());
        t2OutEcnMarks.push_back(t2OutRed->GetTotalMarkPkts());
        r1InEcnMarks.push_back(r1InRed->GetTotalMarkPkts());
      }
      Simulator::Schedule(Seconds(statsSampleInt), QueueStatSample);
  };
  Simulator::Schedule(Seconds(statsSampleInt), QueueStatSample);

  // Schedule simulation end
  Simulator::Stop(Seconds(SIM_TIME + 0.2));

  Simulator::Run();

  // Results
  flowmon->CheckForLostPackets();

  std::map<FlowId, FlowStats> flowResults;
  double sumX = 0.0, sumX2 = 0.0;
  std::vector<double> perFlowThroughput;

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmonHelper.GetClassifier());
  FlowMonitor::FlowStatsContainer stats = flowmon->GetFlowStats();

  for (const auto &st : stats)
  {
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(st.first);
    if ((t.destinationAddress == r1recvIfs.GetAddress(1)) && (t.destinationPort == port))
    {
      double rxBytes = st.second.rxBytes;
      double flowStart = std::max(MEASURE_START, st.second.timeFirstRxPacket.GetSeconds());
      double flowEnd = std::min(MEASURE_END, st.second.timeLastRxPacket.GetSeconds());
      double duration = flowEnd - flowStart;
      if (duration > 0)
      {
        // count only packets received in the interval
        double rxInWindow = rxBytes;
        if (st.second.timeLastRxPacket.GetSeconds() > MEASURE_END)
          rxInWindow -= st.second.rxPackets * (st.second.timeLastRxPacket.GetSeconds() - MEASURE_END) / duration;
        if (st.second.timeFirstRxPacket.GetSeconds() < MEASURE_START)
          rxInWindow -= st.second.rxPackets * (MEASURE_START - st.second.timeFirstRxPacket.GetSeconds()) / duration;

        double tputMbps = (rxInWindow * 8) / (duration * 1e6);
        perFlowThroughput.push_back(tputMbps);
        throughputLog << "Flow " << st.first << ": " << std::fixed << std::setprecision(4)
                      << tputMbps << " Mbps" << std::endl;
        sumX += tputMbps;
        sumX2 += tputMbps * tputMbps;
      }
    }
  }

  // Aggregate fairness index (Jain's fairness)
  double fairness = 0.0;
  if (!perFlowThroughput.empty())
  {
    double sX = std::accumulate(perFlowThroughput.begin(), perFlowThroughput.end(), 0.0);
    double sX2 = std::accumulate(perFlowThroughput.begin(), perFlowThroughput.end(), 0.0,
        [](double acc, double x) { return acc + x * x; }
      );
    fairness = (sX * sX) / (perFlowThroughput.size() * sX2);
  }
  fairnessLog << "Jain's fairness index: " << std::fixed << std::setprecision(6) << fairness << std::endl;

  // Queue statistics (mean, min, max) for measure interval
  auto print_queue_stats = [&](const char* name, const std::vector<uint32_t> &samples,
                               const std::vector<uint64_t>& marks) {
    if (!samples.empty())
    {
      uint32_t min = *std::min_element(samples.begin(), samples.end());
      uint32_t max = *std::max_element(samples.begin(), samples.end());
      double mean = std::accumulate(samples.begin(), samples.end(), 0.0) / samples.size();
      queueLog << name
               << " queue occupancy (bytes): mean=" << (uint32_t)mean
               << ", min=" << min
               << ", max=" << max
               << ", ECN marks=" << (marks.empty() ? 0 : marks.back() - marks.front())
               << std::endl;
    }
  };
  print_queue_stats("T1->T2", t1OutSamples, t1OutEcnMarks);
  print_queue_stats("T2<-T1", t2InSamples, t2InEcnMarks);
  print_queue_stats("T2->R1", t2OutSamples, t2OutEcnMarks);
  print_queue_stats("R1<-T2", r1InSamples, r1InEcnMarks);

  throughputLog.close();
  fairnessLog.close();
  queueLog.close();

  Simulator::Destroy();
  return 0;
}