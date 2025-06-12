#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/queue-disc-module.h"
#include <iomanip>
#include <vector>
#include <numeric>
#include <algorithm>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("DctcpFigure17Sim");

static const uint32_t nSenders = 40;
static const uint32_t flowsToT1T2 = 30;
static const uint32_t flowsToT2R1 = 20;
static const uint32_t totalFlows = 40;
static const double startWindow = 1.0;
static const double convergenceTime = 3.0;
static const double measureInterval = 1.0;

struct FlowStats
{
  double throughputbps;
};

int main (int argc, char *argv[])
{
  // Configure TCP to use DCTCP
  Config::SetDefault ("ns3::TcpL4Protocol::SocketType", StringValue ("ns3::TcpDctcp"));

  // Enable ECN
  Config::SetDefault ("ns3::TcpSocketBase::EcnMode", StringValue ("ClassicEcn"));

  // Node containers: [senders]--T1--T2--R1--receivers
  NodeContainer senders;
  senders.Create (nSenders);
  NodeContainer t1;
  t1.Create (1);
  NodeContainer t2;
  t2.Create (1);
  NodeContainer r1;
  r1.Create (1);
  NodeContainer receivers;
  receivers.Create (nSenders);

  // Connections
  //   senders <-> T1
  //   T1 <-> T2 (bottleneck 1: 10Gbps)
  //   T2 <-> R1 (bottleneck 2: 1Gbps)
  //   R1 <-> receivers

  // PointToPoint helpers
  PointToPointHelper p2pSenderT1;
  p2pSenderT1.SetDeviceAttribute ("DataRate", StringValue ("40Gbps"));
  p2pSenderT1.SetChannelAttribute ("Delay", StringValue ("25us"));

  PointToPointHelper p2pT1T2;
  p2pT1T2.SetDeviceAttribute ("DataRate", StringValue ("10Gbps"));
  p2pT1T2.SetChannelAttribute ("Delay", StringValue ("25us"));

  PointToPointHelper p2pT2R1;
  p2pT2R1.SetDeviceAttribute ("DataRate", StringValue ("1Gbps"));
  p2pT2R1.SetChannelAttribute ("Delay", StringValue ("25us"));

  PointToPointHelper p2pR1Receiver;
  p2pR1Receiver.SetDeviceAttribute ("DataRate", StringValue ("40Gbps"));
  p2pR1Receiver.SetChannelAttribute ("Delay", StringValue ("25us"));

  // Install Devices and Interfaces
  //   senders-T1
  std::vector<NetDeviceContainer> ndcSenderT1 (nSenders);
  for (uint32_t i = 0; i < nSenders; ++i)
    ndcSenderT1[i] = p2pSenderT1.Install (senders.Get (i), t1.Get (0));
  //   T1-T2
  NetDeviceContainer ndcT1T2 = p2pT1T2.Install (t1.Get (0), t2.Get (0));
  //   T2-R1
  NetDeviceContainer ndcT2R1 = p2pT2R1.Install (t2.Get (0), r1.Get (0));
  //   R1-receivers
  std::vector<NetDeviceContainer> ndcR1Receiver (nSenders);
  for (uint32_t i = 0; i < nSenders; ++i)
    ndcR1Receiver[i] = p2pR1Receiver.Install (r1.Get (0), receivers.Get (i));

  // Internet stack
  InternetStackHelper stack;
  stack.Install (senders);
  stack.Install (t1);
  stack.Install (t2);
  stack.Install (r1);
  stack.Install (receivers);

  // Assign IP addresses
  Ipv4AddressHelper addr;
  std::vector<Ipv4InterfaceContainer> ipSenderT1 (nSenders);
  for (uint32_t i = 0; i < nSenders; ++i)
    {
      std::ostringstream subnet;
      subnet << "10.1." << i+1 << ".0";
      addr.SetBase (subnet.str ().c_str (), "255.255.255.0");
      ipSenderT1[i] = addr.Assign (ndcSenderT1[i]);
    }
  addr.SetBase ("10.2.0.0", "255.255.255.0");
  Ipv4InterfaceContainer ipT1T2 = addr.Assign (ndcT1T2);
  addr.SetBase ("10.3.0.0", "255.255.255.0");
  Ipv4InterfaceContainer ipT2R1 = addr.Assign (ndcT2R1);
  std::vector<Ipv4InterfaceContainer> ipR1Receiver (nSenders);
  for (uint32_t i = 0; i < nSenders; ++i)
    {
      std::ostringstream subnet;
      subnet << "10.4." << i+1 << ".0";
      addr.SetBase (subnet.str ().c_str (), "255.255.255.0");
      ipR1Receiver[i] = addr.Assign (ndcR1Receiver[i]);
    }

  // Enable routing
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // RED queues with ECN on T1-T2 and T2-R1
  // Install RED on T1-T2 devices
  TrafficControlHelper tchRed;
  tchRed.SetRootQueueDisc ("ns3::RedQueueDisc",
                           "MinTh", DoubleValue (65 * 1500), // bytes
                           "MaxTh", DoubleValue (195 * 1500), // bytes
                           "LinkBandwidth", StringValue ("10Gbps"),
                           "LinkDelay", StringValue ("25us"),
                           "QueueLimit", UintegerValue (1000),
                           "UseEcn", BooleanValue (true));
  QueueDiscContainer qdcT1T2 = tchRed.Install (ndcT1T2.Get (0));

  TrafficControlHelper tchRed2;
  tchRed2.SetRootQueueDisc ("ns3::RedQueueDisc",
                            "MinTh", DoubleValue (6.5 * 1500), // bytes (for 1Gbps, ~10x less than above)
                            "MaxTh", DoubleValue (19.5 * 1500),
                            "LinkBandwidth", StringValue ("1Gbps"),
                            "LinkDelay", StringValue ("25us"),
                            "QueueLimit", UintegerValue (1000),
                            "UseEcn", BooleanValue (true));
  QueueDiscContainer qdcT2R1 = tchRed2.Install (ndcT2R1.Get (0));

  // Applications
  uint16_t portBase = 50000;
  ApplicationContainer senderApps, receiverApps;

  for (uint32_t i = 0; i < nSenders; ++i)
    {
      // Flow distribution:
      //   - First 30 flows (i=0..29) get routed over T1-T2 link (bottleneck1)
      //   - Last 10 (i=30..39) use T2-R1 (bottleneck2) (but all flows pass both bottlenecks, it's just a per-paper notation)
      // For all flows, connect a sender[i] to receiver[i].
      Address sinkLocalAddress (InetSocketAddress (Ipv4Address::GetAny (), portBase + i));
      PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory", sinkLocalAddress);
      ApplicationContainer sinkApp = sinkHelper.Install (receivers.Get (i));
      sinkApp.Start (Seconds (0.0));
      sinkApp.Stop (Seconds (convergenceTime + measureInterval + 1.0));
      receiverApps.Add (sinkApp);

      Ptr<Socket> srcSocket = Socket::CreateSocket (senders.Get (i), TcpSocketFactory::GetTypeId ());
      InetSocketAddress remoteAddr (ipR1Receiver[i].GetAddress (1), portBase + i);
      remoteAddr.SetTos (0x10); // DCTCP ECN Capable Transport

      Ptr<BulkSendApplication> app = CreateObject<BulkSendApplication> ();
      app->SetAttribute ("Remote", AddressValue (remoteAddr));
      app->SetAttribute ("SendSize", UintegerValue (1448));
      app->SetAttribute ("MaxBytes", UintegerValue (0)); // infinite transfer
      senders.Get (i)->AddApplication (app);
      senderApps.Add (app);

      Time startTime = Seconds (i * (startWindow / nSenders));
      app->SetStartTime (startTime);
      app->SetStopTime (Seconds (convergenceTime + measureInterval + 1.0));
    }

  // FlowMonitor
  FlowMonitorHelper flowHelper;
  Ptr<FlowMonitor> flowMon = flowHelper.InstallAll ();

  // Queue statistics
  Ptr<QueueDisc> t1t2Q = qdcT1T2.Get (0);
  Ptr<QueueDisc> t2r1Q = qdcT2R1.Get (0);

  // Schedule queue stats print
  std::vector<uint32_t> t1t2MarkCount, t2r1MarkCount;
  std::vector<uint32_t> t1t2QueueLen, t2r1QueueLen;

  auto collectQueueStats = [&] ()
  {
      t1t2MarkCount.push_back (t1t2Q->GetStats ().Get (QueueDisc::UNFORCED_MARK_DROPPED));
      t1t2QueueLen.push_back (t1t2Q->GetCurrentSize ().GetValue ());
      t2r1MarkCount.push_back (t2r1Q->GetStats ().Get (QueueDisc::UNFORCED_MARK_DROPPED));
      t2r1QueueLen.push_back (t2r1Q->GetCurrentSize ().GetValue ());
      Simulator::Schedule (Seconds (0.01), collectQueueStats);
  };
  Simulator::Schedule (Seconds (0.0), collectQueueStats);

  // Run simulation
  Simulator::Stop (Seconds (convergenceTime + measureInterval + 1.0));
  Simulator::Run ();

  // Measurement window: between [convergenceTime, convergenceTime + measureInterval]
  double startT = convergenceTime;
  double stopT = convergenceTime + measureInterval;

  // Throughput per flow
  std::map<FlowId, FlowMonitor::FlowStats> stats = flowMon->GetFlowStats ();
  std::map<FlowId, Ipv4FlowClassifier::FiveTuple> classifier = flowHelper.GetClassifier ()->GetFiveTupleForEachFlowId ();
  std::vector<double> throughput;
  throughput.reserve (totalFlows);

  std::cout << std::fixed << std::setprecision (3) << "Per-flow Throughput (Mbps):\n";

  for (auto const& flow : stats)
    {
      FlowId id = flow.first;
      auto tupleIter = classifier.find (id);
      if (tupleIter == classifier.end ())
        continue;
      Ipv4FlowClassifier::FiveTuple t = tupleIter->second;
      // Sender->receiver flows
      bool match = false;
      uint32_t flowIdx = totalFlows;
      for (uint32_t i = 0; i < nSenders; ++i)
        {
          if (t.sourceAddress == ipSenderT1[i].GetAddress (0) &&
              t.destinationAddress == ipR1Receiver[i].GetAddress (1) &&
              t.destinationPort == portBase + i)
            {
              match = true;
              flowIdx = i;
              break;
            }
        }
      if (!match) continue;

      double b = (flow.second.rxBytes * 8.) / 1e6 / (stopT - startT); // Mbps
      throughput.push_back (b);

      std::cout << "Flow " << flowIdx << ": " << b << " Mbps\n";
    }

  // Aggregate statistics
  double aggThroughput = std::accumulate (throughput.begin (), throughput.end (), 0.0);

  double fairness = 0.0;
  if (!throughput.empty ())
    {
      double sum = 0, sumsq = 0;
      for (double x : throughput)
        {
          sum += x;
          sumsq += x * x;
        }
      fairness = (sum*sum) / (throughput.size () * sumsq);
    }
  std::cout << "Aggregate Throughput: " << aggThroughput << " Mbps\n";
  std::cout << "Jain's Fairness Index: " << fairness << "\n";

  // Queue statistics reporting (average queue size, avg # marks per second)
  double avgT1T2Mark = std::accumulate (t1t2MarkCount.begin (), t1t2MarkCount.end (), 0.0) / t1t2MarkCount.size ();
  double avgT2R1Mark = std::accumulate (t2r1MarkCount.begin (), t2r1MarkCount.end (), 0.0) / t2r1MarkCount.size ();
  double avgT1T2QLen = std::accumulate (t1t2QueueLen.begin (), t1t2QueueLen.end (), 0.0) / t1t2QueueLen.size ();
  double avgT2R1QLen = std::accumulate (t2r1QueueLen.begin (), t2r1QueueLen.end (), 0.0) / t2r1QueueLen.size ();

  std::cout << "T1-T2 avg ECN marks/sec: " << avgT1T2Mark << ", avg queue size: " << avgT1T2QLen / 1500.0 << " pkts\n";
  std::cout << "T2-R1 avg ECN marks/sec: " << avgT2R1Mark << ", avg queue size: " << avgT2R1QLen / 1500.0 << " pkts\n";

  Simulator::Destroy ();
  return 0;
}