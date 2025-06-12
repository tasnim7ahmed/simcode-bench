#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/queue-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

int main(int argc, char *argv[]) {

  uint32_t maxBytes = 0;
  std::string queueDiscType = "ns3::TbfQueueDisc";
  uint32_t queueSize1 = 10000;
  uint32_t queueSize2 = 10000;
  std::string arrivalRate = "50000bps";
  std::string peakRate = "100000bps";

  CommandLine cmd;
  cmd.AddValue("queueSize1", "Size of the first bucket in bytes", queueSize1);
  cmd.AddValue("queueSize2", "Size of the second bucket in bytes", queueSize2);
  cmd.AddValue("arrivalRate", "Token arrival rate", arrivalRate);
  cmd.AddValue("peakRate", "Peak rate", peakRate);

  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);

  NodeContainer nodes;
  nodes.Create(2);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
  pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

  NetDeviceContainer devices;
  devices = pointToPoint.Install(nodes);

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");

  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  QueueDiscContainer queueDiscs;
  TrafficControlHelper tch;
  tch.SetRootQueueDisc(queueDiscType,
                       "MaxSize", StringValue("1000p"),
                       "BucketSize1", UintegerValue(queueSize1),
                       "BucketSize2", UintegerValue(queueSize2),
                       "TokenArrivalRate", StringValue(arrivalRate),
                       "PeakRate", StringValue(peakRate));
  queueDiscs = tch.Install(devices.Get(0));

  uint16_t port = 9;
  OnOffHelper onoff("ns3::UdpSocketFactory",
                     Address(InetSocketAddress(interfaces.GetAddress(1), port)));
  onoff.SetConstantRate(DataRate("5Mbps"));

  ApplicationContainer app = onoff.Install(nodes.Get(0));
  app.Start(Seconds(1.0));
  app.Stop(Seconds(10.0));

  PacketSinkHelper sink("ns3::UdpSocketFactory",
                         InetSocketAddress(Ipv4Address::GetAny(), port));
  app = sink.Install(nodes.Get(1));
  app.Start(Seconds(1.0));
  app.Stop(Seconds(10.0));

  AsciiTraceHelper ascii;
  QueueDisc::GetInstance(queueDiscs.Get(0))->TraceQueueDiscEnQueue(ascii.CreateFileStream("tbf-enqueue.tr"));
  QueueDisc::GetInstance(queueDiscs.Get(0))->TraceQueueDiscDequeue(ascii.CreateFileStream("tbf-dequeue.tr"));

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();

  Ptr<QueueDisc> qd = QueueDisc::GetInstance(queueDiscs.Get(0));
  std::cout << "Bytes in first bucket: " << qd->GetStats().BytesInFirstBucket.GetValue() << std::endl;
  std::cout << "Bytes in second bucket: " << qd->GetStats().BytesInSecondBucket.GetValue() << std::endl;

  Simulator::Destroy();
  return 0;
}