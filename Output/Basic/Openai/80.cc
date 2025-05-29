#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("TbfExample");

void
TraceBucketTokens (std::string context, uint32_t tokens)
{
  std::cout << Simulator::Now ().GetSeconds () << "s " << context << " " << tokens << std::endl;
}

int 
main (int argc, char *argv[])
{
  uint32_t firstBucketSize = 10000; // bytes
  uint32_t secondBucketSize = 2000; // bytes
  uint32_t tokenRate = 50000; // bps
  uint32_t peakRate = 100000; // bps

  CommandLine cmd;
  cmd.AddValue ("firstBucketSize", "Size of first token bucket in bytes", firstBucketSize);
  cmd.AddValue ("secondBucketSize", "Size of second token bucket in bytes", secondBucketSize);
  cmd.AddValue ("tokenRate", "Token arrival rate (bps)", tokenRate);
  cmd.AddValue ("peakRate", "Peak rate (bps)", peakRate);
  cmd.Parse (argc, argv);

  NodeContainer nodes;
  nodes.Create (2);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("150Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer devices = p2p.Install (nodes);

  InternetStackHelper stack;
  stack.Install (nodes);

  TrafficControlHelper tch;
  uint16_t handle = tch.SetRootQueueDisc ("ns3::TbfQueueDisc",
    "FirstBucketSize", UintegerValue (firstBucketSize),
    "SecondBucketSize", UintegerValue (secondBucketSize),
    "Rate", DataRateValue (DataRate (tokenRate)),
    "PeakRate", DataRateValue (DataRate (peakRate)),
    "Burst", UintegerValue (firstBucketSize)
  );
  QueueDiscContainer qdiscs = tch.Install (devices);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  uint16_t port = 9;
  OnOffHelper onoff ("ns3::UdpSocketFactory", InetSocketAddress (interfaces.GetAddress (1), port));
  onoff.SetAttribute ("DataRate", DataRateValue (DataRate ("20Mbps")));
  onoff.SetAttribute ("PacketSize", UintegerValue (1024));
  onoff.SetAttribute ("StartTime", TimeValue (Seconds (1.0)));
  onoff.SetAttribute ("StopTime", TimeValue (Seconds (9.0)));
  ApplicationContainer app = onoff.Install (nodes.Get (0));

  PacketSinkHelper sink ("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), port));
  ApplicationContainer sinkApp = sink.Install (nodes.Get (1));
  sinkApp.Start (Seconds (0.0));
  sinkApp.Stop (Seconds (10.0));

  Ptr<TbfQueueDisc> tbf = DynamicCast<TbfQueueDisc> (qdiscs.Get (0));
  if (tbf)
    {
      tbf->TraceConnectWithoutContext ("FirstBucketTokens",
        MakeCallback ([] (uint32_t tokens) { TraceBucketTokens ("FirstBucketTokens", tokens); }));
      tbf->TraceConnectWithoutContext ("SecondBucketTokens",
        MakeCallback ([] (uint32_t tokens) { TraceBucketTokens ("SecondBucketTokens", tokens); }));
    }

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();

  if (tbf)
    {
      std::cout << "TBF statistics:" << std::endl;
      std::cout << "  Packets enqueued: " << tbf->GetStats ().nPacketsEnqueued << std::endl;
      std::cout << "  Packets dequeued: " << tbf->GetStats ().nPacketsDequeued << std::endl;
      std::cout << "  Packets dropped:  " << tbf->GetStats ().nPacketsDropped << std::endl;
    }

  Simulator::Destroy ();
  return 0;
}