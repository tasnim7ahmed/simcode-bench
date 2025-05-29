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
#include <algorithm>
#include <numeric>
#include <cmath>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("DctcpFigure17");

struct FlowStats
{
  double throughput;
};

void
CollectQueueStats (Ptr<QueueDisc> queue, std::string bottleneckName)
{
  QueueDisc::Stats stats = queue->GetStats ();
  std::cout << bottleneckName << " queue statistics:\n";
  std::cout << "  Packets marked ECN: " << stats.nTotalMarkedPackets << std::endl;
  std::cout << "  Packets dropped:    " << stats.nTotalDroppedPackets << std::endl;
  std::cout << "  Bytes marked ECN:   " << stats.nTotalMarkedBytes << std::endl;
  std::cout << "  Bytes dropped:      " << stats.nTotalDroppedBytes << std::endl;
}

int
main (int argc, char *argv[])
{
  LogComponentEnable ("DctcpFigure17", LOG_LEVEL_INFO);

  uint32_t numSenders = 40;

  CommandLine cmd;
  cmd.Parse (argc, argv);

  // Topology:
  // [N0-N29]---T1---|10Gbps|---T2---|1Gbps|---R1---[N30-N39]

  NodeContainer senders1, senders2, t1, t2, r1, receivers;
  senders1.Create (30);
  senders2.Create (10);
  t1.Create (1);
  t2.Create (1);
  r1.Create (1);
  receivers.Create (numSenders);

  InternetStackHelper stack;
  stack.Install (senders1);
  stack.Install (senders2);
  stack.Install (t1);
  stack.Install (t2);
  stack.Install (r1);
  stack.Install (receivers);

  // Point-to-point helpers
  // Connections:
  // [S0-29] <-> T1
  // [S30-39] <-> R1
  // T1 <-> T2 -- 10Gbps bottleneck
  // T2 <-> R1 -- 1Gbps bottleneck

  // S[0-29] <-> T1
  PointToPointHelper h2t1;
  h2t1.SetDeviceAttribute ("DataRate", StringValue ("10Gbps"));
  h2t1.SetChannelAttribute ("Delay", StringValue ("10us"));

  // S[30-39] <-> R1
  PointToPointHelper h2r1;
  h2r1.SetDeviceAttribute ("DataRate", StringValue ("10Gbps"));
  h2r1.SetChannelAttribute ("Delay", StringValue ("10us"));

  // T1 <-> T2 -- 10 Gbps bottleneck
  PointToPointHelper t1t2;
  t1t2.SetDeviceAttribute ("DataRate", StringValue ("10Gbps"));
  t1t2.SetChannelAttribute ("Delay", StringValue ("50us"));

  // T2 <-> R1 -- 1 Gbps bottleneck
  PointToPointHelper t2r1;
  t2r1.SetDeviceAttribute ("DataRate", StringValue ("1Gbps"));
  t2r1.SetChannelAttribute ("Delay", StringValue ("50us"));

  // Devices and interfaces
  std::vector<NetDeviceContainer> senderToT1Devs (30);
  std::vector<NetDeviceContainer> senderToR1Devs (10);
  NetDeviceContainer t1t2Devs, t2r1Devs;

  // Install on sender1<->T1
  for (uint32_t i = 0; i < 30; ++i)
    {
      senderToT1Devs[i] = h2t1.Install (NodeContainer (senders1.Get (i), t1.Get (0)));
    }
  // Install on sender2<->R1
  for (uint32_t i = 0; i < 10; ++i)
    {
      senderToR1Devs[i] = h2r1.Install (NodeContainer (senders2.Get (i), r1.Get (0)));
    }
  // T1 <-> T2
  t1t2Devs = t1t2.Install (NodeContainer (t1.Get (0), t2.Get (0)));
  // T2 <-> R1
  t2r1Devs = t2r1.Install (NodeContainer (t2.Get (0), r1.Get (0)));

  // Assign IP addresses
  Ipv4AddressHelper ipv4;
  std::vector<Ipv4InterfaceContainer> senderToT1Ifaces (30), senderToR1Ifaces (10);
  Ipv4InterfaceContainer t1t2Ifaces, t2r1Ifaces;

  for (uint32_t i = 0; i < 30; ++i)
    {
      std::ostringstream subnet;
      subnet << "10.1." << i+1 << ".0";
      ipv4.SetBase (subnet.str ().c_str (), "255.255.255.0");
      senderToT1Ifaces[i] = ipv4.Assign (senderToT1Devs[i]);
    }
  for (uint32_t i = 0; i < 10; ++i)
    {
      std::ostringstream subnet;
      subnet << "10.2." << i+1 << ".0";
      ipv4.SetBase (subnet.str ().c_str (), "255.255.255.0");
      senderToR1Ifaces[i] = ipv4.Assign (senderToR1Devs[i]);
    }
  ipv4.SetBase ("10.3.0.0", "255.255.255.0");
  t1t2Ifaces = ipv4.Assign (t1t2Devs);

  ipv4.SetBase ("10.4.0.0", "255.255.255.0");
  t2r1Ifaces = ipv4.Assign (t2r1Devs);

  // Routing
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // RED + ECN on T1<->T2 and T2<->R1
  TrafficControlHelper tchBottleneck;
  tchBottleneck.SetRootQueueDisc ("ns3::RedQueueDisc",
                                  "MinTh", DoubleValue (15 * 1500),
                                  "MaxTh", DoubleValue (30 * 1500),
                                  "LinkBandwidth", StringValue ("10Gbps"),
                                  "LinkDelay", StringValue ("50us"),
                                  "Mode", StringValue ("QUEUE_MODE_BYTES"),
                                  "MeanPktSize", UintegerValue (1500),
                                  "UseEcn", BooleanValue (true));

  // On t1t2 link (use RED with ECN on T1->T2 direction)
  QueueDiscContainer qdT1 = tchBottleneck.Install (t1t2Devs.Get (0));
  // On t2r1 link (RED with ECN on T2->R1 direction, 1Gbps here)
  TrafficControlHelper tchT2r1;
  tchT2r1.SetRootQueueDisc ("ns3::RedQueueDisc",
                            "MinTh", DoubleValue (5 * 1500),
                            "MaxTh", DoubleValue (10 * 1500),
                            "LinkBandwidth", StringValue ("1Gbps"),
                            "LinkDelay", StringValue ("50us"),
                            "Mode", StringValue ("QUEUE_MODE_BYTES"),
                            "MeanPktSize", UintegerValue (1500),
                            "UseEcn", BooleanValue (true));
  QueueDiscContainer qdT2 = tchT2r1.Install (t2r1Devs.Get (0));

  // DCTCP TCP congestion control
  Config::SetDefault ("ns3::TcpL4Protocol::SocketType", StringValue ("ns3::TcpDctcp"));

  // Flows: each sender -> one receiver, 40 in total
  uint16_t portBase = 5000;
  ApplicationContainer sinkApps, sourceApps;
  std::vector<Ptr<Socket>> sourceSockets (numSenders);

  // Receivers: run PacketSinkApplication on the receivers
  for (uint32_t i = 0; i < numSenders; ++i)
    {
      Address sinkAddress (InetSocketAddress (receivers.Get (i)->GetObject<Ipv4> ()->GetAddress (1,0).GetLocal (), portBase + i));
      PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory", sinkAddress);
      ApplicationContainer sink = sinkHelper.Install (receivers.Get (i));
      sink.Start (Seconds (0.0));
      sink.Stop (Seconds (6.0));
      sinkApps.Add (sink);
    }

  // Map: 0-29 senders use T1, 30-39 senders use R1 directly
  for (uint32_t i = 0; i < numSenders; ++i)
    {
      // Each sender will send to its corresponding receiver
      Ptr<Node> sender;
      Ipv4Address receiverAddr;
      if (i < 30)
        {
          sender = senders1.Get (i);
          receiverAddr = receivers.Get (i)->GetObject<Ipv4> ()->GetAddress (1,0).GetLocal ();
        }
      else
        {
          sender = senders2.Get (i - 30);
          receiverAddr = receivers.Get (i)->GetObject<Ipv4> ()->GetAddress (1,0).GetLocal ();
        }

      OnOffHelper onoff ("ns3::TcpSocketFactory", InetSocketAddress (receiverAddr, portBase + i));
      onoff.SetAttribute ("DataRate", StringValue ("500Mbps"));    // High, limited by link/queue
      onoff.SetAttribute ("PacketSize", UintegerValue (1500));
      onoff.SetAttribute ("MaxBytes", UintegerValue (0));
      onoff.SetAttribute ("StartTime", TimeValue (Seconds (1.0 * i / numSenders)));
      onoff.SetAttribute ("StopTime", TimeValue (Seconds (5.0)));

      sourceApps.Add (onoff.Install (sender));
    }

  // Run simulation
  Simulator::Stop (Seconds (6.0));
  Simulator::Run ();

  // Throughput measurement per flow (during [4s, 5s])
  std::vector<double> throughputs (numSenders, 0.0);
  double interval = 1.0;
  double startTime = 4.0;
  double endTime = 5.0;

  for (uint32_t i = 0; i < numSenders; ++i)
    {
      Ptr<PacketSink> sink = DynamicCast<PacketSink> (sinkApps.Get (i));
      // Collect stats: bytes received until t_end, minus bytes received until t_start
      uint64_t totalRxStart = 0, totalRxEnd = 0;
      Simulator::Schedule (Seconds (startTime), &PacketSink::ResetTotalRx, sink);
      Simulator::Schedule (Seconds (endTime),
          [&throughputs, i, sink, interval]()
          {
            uint64_t bytes = sink->GetTotalRx ();
            // Throughput in Mbps
            throughputs[i] = (bytes * 8.0) / (1000000.0 * interval);
          });
    }

  Simulator::Run ();

  // Output per-flow throughput
  std::cout << "Flow\tThroughput (Mbps)\n";
  for (uint32_t i = 0; i < numSenders; ++i)
    {
      std::cout << i << "\t" << std::fixed << std::setprecision (2) << throughputs[i] << std::endl;
    }

  // Jain's fairness index
  double sum = std::accumulate (throughputs.begin (), throughputs.end (), 0.0);
  double sumSq = std::accumulate (throughputs.begin (), throughputs.end (), 0.0,
                                  [](double accum, double val) { return accum + val * val; });

  double fairness = (sum * sum) / (numSenders * sumSq);
  std::cout << "Jain's Fairness Index: " << std::fixed << std::setprecision (4) << fairness << std::endl;
  std::cout << "Aggregate Throughput: " << sum << " Mbps" << std::endl;

  // Output queue stats
  CollectQueueStats (qdT1.Get (0), "T1-T2");
  CollectQueueStats (qdT2.Get (0), "T2-R1");

  Simulator::Destroy ();
  return 0;
}