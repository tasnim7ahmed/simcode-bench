#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/queue.h"
#include "ns3/queue-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/internet-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("TbfExample");

int main (int argc, char *argv[])
{
  CommandLine cmd;
  uint32_t maxBytesFirstBucket = 10000;
  uint32_t maxBytesSecondBucket = 20000;
  uint32_t tokenArrivalRate = 50000;
  uint32_t peakRate = 100000;

  cmd.AddValue ("maxBytesFirstBucket", "Max bytes of the first bucket", maxBytesFirstBucket);
  cmd.AddValue ("maxBytesSecondBucket", "Max bytes of the second bucket", maxBytesSecondBucket);
  cmd.AddValue ("tokenArrivalRate", "Token arrival rate", tokenArrivalRate);
  cmd.AddValue ("peakRate", "Peak rate", peakRate);

  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);
  LogComponent::SetLogLevel(LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create (2);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("100Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer devices;
  devices = pointToPoint.Install (nodes);

  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  TrafficControlHelper tch;
  QueueDiscContainer qdiscs;
  TbfDiscHelper tbf;

  tbf.SetAttribute ("MaxBytesFirstBucket", UintegerValue (maxBytesFirstBucket));
  tbf.SetAttribute ("MaxBytesSecondBucket", UintegerValue (maxBytesSecondBucket));
  tbf.SetAttribute ("TokenArrivalRate", UintegerValue (tokenArrivalRate));
  tbf.SetAttribute ("PeakRate", UintegerValue (peakRate));
  tch.SetRootQueueDisc ("ns3::TbfDisc", "MaxBytesFirstBucket", UintegerValue (maxBytesFirstBucket), "MaxBytesSecondBucket", UintegerValue (maxBytesSecondBucket), "TokenArrivalRate", UintegerValue (tokenArrivalRate), "PeakRate", UintegerValue (peakRate));
  qdiscs = tch.Install (devices.Get (1));

  Ptr<QueueDisc> queue = qdiscs.Get (0);

  AsciiTraceHelper ascii;
  queue->TraceConnectWithoutContext ("BytesInFirstBucket", ascii.CreateFileStream ("tbf-first-bucket.txt"));
  queue->TraceConnectWithoutContext ("BytesInSecondBucket", ascii.CreateFileStream ("tbf-second-bucket.txt"));

  uint16_t port = 9;
  OnOffHelper onoff ("ns3::UdpSocketFactory",
                     Address (InetSocketAddress (interfaces.GetAddress (1), port)));
  onoff.SetAttribute ("OnTime",  StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
  onoff.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
  onoff.SetAttribute ("DataRate", DataRateValue (DataRate ("50Mbps")));
  onoff.SetAttribute ("PacketSize", UintegerValue (1024));

  ApplicationContainer apps = onoff.Install (nodes.Get (0));
  apps.Start (Seconds (1.0));
  apps.Stop (Seconds (10.0));

  PacketSinkHelper sink ("ns3::UdpSocketFactory",
                         InetSocketAddress (Ipv4Address::GetAny (), port));
  apps = sink.Install (nodes.Get (1));
  apps.Start (Seconds (1.0));
  apps.Stop (Seconds (10.0));

  Simulator::Stop (Seconds (10.0));

  Simulator::Run ();

  Ptr<TbfDisc> tbfQueue = DynamicCast<TbfDisc> (queue);
  if (tbfQueue)
    {
      std::cout << "TBF Queue Statistics:" << std::endl;
      std::cout << "  Packets Dropped: " << tbfQueue->GetStats ().packetsDropped << std::endl;
      std::cout << "  Packets Passed: " << tbfQueue->GetStats ().packetsPassed << std::endl;
      std::cout << "  Bytes Dropped: " << tbfQueue->GetStats ().bytesDropped << std::endl;
      std::cout << "  Bytes Passed: " << tbfQueue->GetStats ().bytesPassed << std::endl;
    }
  else
    {
      std::cerr << "Error: Could not retrieve TBF queue statistics." << std::endl;
    }

  Simulator::Destroy ();
  return 0;
}