#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/queue.h"
#include "ns3/queue-disc.h"
#include "ns3/traffic-control-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/packet.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/internet-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("TbfExample");

int main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.SetCommandName ("tbf-example");

  uint32_t firstBucketSize = 1024;
  uint32_t secondBucketSize = 2048;
  std::string tokenArrivalRate = "1Mbps";
  std::string peakRate = "10Mbps";
  std::string onOffRate = "5Mbps";
  std::string appDataRate = "5Mbps";

  cmd.AddValue ("firstBucketSize", "Size of the first bucket in bytes", firstBucketSize);
  cmd.AddValue ("secondBucketSize", "Size of the second bucket in bytes", secondBucketSize);
  cmd.AddValue ("tokenArrivalRate", "Token arrival rate", tokenArrivalRate);
  cmd.AddValue ("peakRate", "Peak rate", peakRate);
  cmd.AddValue ("onOffRate", "On Off Application Rate", onOffRate);
  cmd.AddValue ("appDataRate", "Application Data Rate", appDataRate);

  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);
  LogComponent::SetDefaultLogLevel (LogLevel::LOG_LEVEL_INFO);
  LogComponent::SetPrintfAll (true);

  NodeContainer nodes;
  nodes.Create (2);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
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

  tch.SetRootQueueDisc ("ns3::TbfQueueDisc",
                         "MaxSize", StringValue ("65535p"),
                         "FirstBucketSize", UintegerValue (firstBucketSize),
                         "SecondBucketSize", UintegerValue (secondBucketSize),
                         "TokenArrivalRate", StringValue (tokenArrivalRate),
                         "PeakRate", StringValue (peakRate));

  qdiscs = tch.Install (devices.Get (1));

  uint16_t port = 9;  // Discard port (RFC 863)

  OnOffHelper onoff ("ns3::UdpSocketFactory",
                     Address (InetSocketAddress (interfaces.GetAddress (1), port)));
  onoff.SetConstantRate (DataRate (onOffRate));

  ApplicationContainer apps = onoff.Install (nodes.Get (0));
  apps.Start (Seconds (1.0));
  apps.Stop (Seconds (10.0));

  PacketSinkHelper sink ("ns3::UdpSocketFactory",
                         InetSocketAddress (Ipv4Address::GetAny (), port));
  apps = sink.Install (nodes.Get (1));
  apps.Start (Seconds (1.0));
  apps.Stop (Seconds (10.0));

  // Token bucket tracing
  AsciiTraceHelper ascii;
  Ptr<OutputStreamWrapper> stream1 = ascii.CreateFileStream ("tbf-first-bucket.txt");
  qdiscs.Get (0)->TraceConnectWithoutContext ("FirstBucketSize",
                                             "TbfExample",
                                             MakeBoundCallback (&OutputStreamWrapper::Print, stream1));
  Ptr<OutputStreamWrapper> stream2 = ascii.CreateFileStream ("tbf-second-bucket.txt");
  qdiscs.Get (0)->TraceConnectWithoutContext ("SecondBucketSize",
                                             "TbfExample",
                                             MakeBoundCallback (&OutputStreamWrapper::Print, stream2));

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();

  tch.Destroy (qdiscs);

  Simulator::Destroy ();
  return 0;
}