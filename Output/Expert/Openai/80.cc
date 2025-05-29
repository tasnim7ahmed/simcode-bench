#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/queue-disc-module.h"

using namespace ns3;

static void
TraceTokens (Ptr<Object> tbf, std::string attr, std::string bucketName)
{
  double tokens = tbf->GetObject<QueueDisc>()->GetAttribute (attr, DoubleValue ()).Get ().GetDouble ();
  Simulator::Schedule (Seconds (0.1), &TraceTokens, tbf, attr, bucketName);
  std::cout << Simulator::Now ().GetSeconds () << "s: " << bucketName << " tokens = " << tokens << std::endl;
}

int main (int argc, char *argv[])
{
  double firstBucketSize = 8000; // in bytes
  double secondBucketSize = 4000; // in bytes
  double rate = 512000; // in bps
  double peakRate = 1024000; // in bps
  uint32_t pktSize = 1024; // bytes
  double simTime = 5.0;

  CommandLine cmd;
  cmd.AddValue ("firstBucketSize", "TBF burst size (bytes)", firstBucketSize);
  cmd.AddValue ("secondBucketSize", "TBF second bucket size (bytes)", secondBucketSize);
  cmd.AddValue ("rate", "Token arrival rate (bps)", rate);
  cmd.AddValue ("peakRate", "Peak rate (bps)", peakRate);
  cmd.AddValue ("pktSize", "Packet size (bytes)", pktSize);
  cmd.AddValue ("simTime", "Simulation time (s)", simTime);
  cmd.Parse (argc, argv);

  NodeContainer nodes;
  nodes.Create (2);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer devices = p2p.Install (nodes);

  TrafficControlHelper tch;
  uint16_t handle = tch.SetRootQueueDisc ("ns3::TbfQueueDisc",
    "Burst", DoubleValue (firstBucketSize),
    "MaxSize", QueueSizeValue (QueueSize (QueueSizeUnit::BYTES, firstBucketSize)),
    "PeakRate", DataRateValue (DataRate ((uint64_t)peakRate)),
    "Rate", DataRateValue (DataRate ((uint64_t)rate)),
    "MaxTokens", DoubleValue (firstBucketSize),
    "SecondBurst", DoubleValue (secondBucketSize),
    "MaxTokensInSecondBucket", DoubleValue (secondBucketSize)
  );
  QueueDiscContainer qdiscs = tch.Install (devices.Get (0));

  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  uint16_t port = 9;

  // Sink
  Address sinkAddress (InetSocketAddress (interfaces.GetAddress (1), port));
  PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory", sinkAddress);
  ApplicationContainer sinkApp = sinkHelper.Install (nodes.Get (1));
  sinkApp.Start (Seconds (0.0));
  sinkApp.Stop (Seconds (simTime));

  // Source (OnOff)
  OnOffHelper onoff ("ns3::TcpSocketFactory", sinkAddress);
  onoff.SetAttribute ("DataRate", DataRateValue (DataRate ("20Mbps")));
  onoff.SetAttribute ("PacketSize", UintegerValue (pktSize));
  onoff.SetAttribute ("StartTime", TimeValue (Seconds (0.1)));
  onoff.SetAttribute ("StopTime", TimeValue (Seconds (simTime)));

  onoff.Install (nodes.Get (0));

  // Trace tokens in both buckets
  Ptr<QueueDisc> tbf = qdiscs.Get (0);

  Config::ConnectWithoutContext (tbf->GetInstanceTypeId ().GetName () + "/Tokens", MakeCallback (
    [] (const double &tokens) {
      // No-op. We use our own polling-based manual tracing below.
    }));

  Simulator::ScheduleNow (&TraceTokens, tbf, "Tokens", "FirstBucket");
  Simulator::ScheduleNow (&TraceTokens, tbf, "SecondTokens", "SecondBucket");

  Simulator::Stop (Seconds (simTime));
  Simulator::Run ();

  std::cout << "TBF statistics at the end of simulation:" << std::endl;
  std::cout << "  Packets enqueued:    " << tbf->GetTotalReceivedPackets () << std::endl;
  std::cout << "  Packets dequeued:    " << tbf->GetTotalTransmittedPackets () << std::endl;
  std::cout << "  Packets dropped:     " << tbf->GetTotalDroppedPackets () << std::endl;
  std::cout << "  Bytes enqueued:      " << tbf->GetTotalReceivedBytes () << std::endl;
  std::cout << "  Bytes dequeued:      " << tbf->GetTotalTransmittedBytes () << std::endl;
  std::cout << "  Bytes dropped:       " << tbf->GetTotalDroppedBytes () << std::endl;

  Simulator::Destroy ();
  return 0;
}