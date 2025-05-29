#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("TbfExample");

static void
TokenBucketTracer(std::string context, uint32_t tokens)
{
  std::cout << Simulator::Now ().GetSeconds () << "s [" << context << "] Tokens: " << tokens << std::endl;
}

int
main (int argc, char *argv[])
{
  // Default TBF parameters
  uint32_t bucketSize = 16000; // bytes
  uint32_t peakBucketSize = 16000; // bytes
  DataRate tokenRate ("1Mbps");
  DataRate peakRate ("2Mbps");

  CommandLine cmd;
  cmd.AddValue ("bucketSize", "TBF first bucket size (bytes)", bucketSize);
  cmd.AddValue ("peakBucketSize", "TBF second bucket (peak) size (bytes)", peakBucketSize);
  cmd.AddValue ("tokenRate", "Token arrival rate (e.g., 1Mbps)", tokenRate);
  cmd.AddValue ("peakRate", "Peak rate (e.g., 2Mbps)", peakRate);
  cmd.Parse (argc, argv);

  NodeContainer nodes;
  nodes.Create (2);

  // Point-to-point link
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

  // Install devices
  NetDeviceContainer devices = p2p.Install (nodes);

  // Install Internet stack
  InternetStackHelper stack;
  stack.Install (nodes);

  // Assign IP addresses
  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  // Set up TBF on device 0 -> 1
  TrafficControlHelper tch;
  uint16_t handle = tch.SetRootQueueDisc ("ns3::TbfQueueDisc",
                                          "Rate", DataRateValue(tokenRate),
                                          "PeakRate", DataRateValue(peakRate),
                                          "Burst", UintegerValue (bucketSize),
                                          "PeakBurst", UintegerValue (peakBucketSize));
  QueueDiscContainer qdiscs = tch.Install (devices.Get (0));

  // Tracing: Token count in each bucket
  Ptr<QueueDisc> tbf = qdiscs.Get (0);
  tbf->TraceConnect ("Tokens", "first-bucket", MakeCallback (&TokenBucketTracer));
  tbf->TraceConnect ("Tokens", "second-bucket", MakeCallback (&TokenBucketTracer));

  // OnOff application (UDP)
  uint16_t port = 9000;
  OnOffHelper onoff ("ns3::UdpSocketFactory", InetSocketAddress (interfaces.GetAddress (1), port));
  onoff.SetAttribute ("DataRate", StringValue ("5Mbps"));
  onoff.SetAttribute ("PacketSize", UintegerValue (1024));
  onoff.SetAttribute ("StartTime", TimeValue (Seconds (1.0)));
  onoff.SetAttribute ("StopTime", TimeValue (Seconds (10.0)));

  ApplicationContainer apps = onoff.Install (nodes.Get (0));

  // Add sink on node 1
  PacketSinkHelper sink ("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), port));
  ApplicationContainer sinkApp = sink.Install (nodes.Get (1));
  sinkApp.Start (Seconds (0.0));
  sinkApp.Stop (Seconds (11.0));

  Simulator::Stop (Seconds (11.0));

  Simulator::Run ();

  // Output TBF statistics
  std::cout << "========== TBF Queue Stats ==========" << std::endl;
  if (tbf)
    {
      std::cout << "Total packets enqueued: " << tbf->GetStats ().nPacketsEnqueued << std::endl;
      std::cout << "Total packets dequeued: " << tbf->GetStats ().nPacketsDequeued << std::endl;
      std::cout << "Total packets dropped: " << tbf->GetStats ().nPacketsDropped << std::endl;
      std::cout << "Bytes enqueued: " << tbf->GetStats ().nBytesEnqueued << std::endl;
      std::cout << "Bytes dequeued: " << tbf->GetStats ().nBytesDequeued << std::endl;
      std::cout << "Bytes dropped: " << tbf->GetStats ().nBytesDropped << std::endl;
    }
  std::cout << "=====================================" << std::endl;

  Simulator::Destroy ();
  return 0;
}