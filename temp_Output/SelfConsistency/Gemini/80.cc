#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/queue-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/internet-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TbfExample");

int main(int argc, char *argv[]) {
  bool enablePcap = false;
  std::string animFile = "tbf-example.xml";

  // TBF parameters
  uint32_t firstBucketSize = 3000;
  uint32_t secondBucketSize = 3000;
  std::string tokenArrivalRate = "10Mbps";
  std::string peakRate = "100Mbps";

  // Command line arguments
  CommandLine cmd;
  cmd.AddValue("enablePcap", "Enable PCAP tracing", enablePcap);
  cmd.AddValue("animFile", "File Name for Animation Output", animFile);
  cmd.AddValue("firstBucketSize", "Size of the first token bucket in bytes", firstBucketSize);
  cmd.AddValue("secondBucketSize", "Size of the second token bucket in bytes", secondBucketSize);
  cmd.AddValue("tokenArrivalRate", "Token arrival rate", tokenArrivalRate);
  cmd.AddValue("peakRate", "Peak rate", peakRate);
  cmd.Parse(argc, argv);

  // Create nodes
  NodeContainer nodes;
  nodes.Create(2);

  // Point-to-point channel
  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
  pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

  // Install NetDevices
  NetDeviceContainer devices;
  devices = pointToPoint.Install(nodes);

  // Install the Internet stack
  InternetStackHelper stack;
  stack.Install(nodes);

  // Assign IP addresses
  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  // Create OnOff application (Traffic Generator)
  OnOffHelper onoff("ns3::UdpSocketFactory", Address(InetSocketAddress(interfaces.GetAddress(1), 9)));
  onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
  onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
  onoff.SetAttribute("DataRate", StringValue("50Mbps")); // Send at 50 Mbps
  onoff.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer app = onoff.Install(nodes.Get(0));
  app.Start(Seconds(1.0));
  app.Stop(Seconds(10.0));

  // Create a sink application on the receiving node
  PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(interfaces.GetAddress(1), 9));
  ApplicationContainer sinkApp = sink.Install(nodes.Get(1));
  sinkApp.Start(Seconds(1.0));
  sinkApp.Stop(Seconds(10.0));

  // Install TBF queue on node 0, outgoing interface
  TrafficControlHelper tch;
  tch.SetRootQueueDisc("ns3::TokenBucketFilter",
                       "MaxSize", StringValue("1000000p"), // Dummy value, TBF doesn't use MaxSize
                       "FirstBucketSize", UintegerValue(firstBucketSize),
                       "SecondBucketSize", UintegerValue(secondBucketSize),
                       "TokenArrivalRate", StringValue(tokenArrivalRate),
                       "PeakRate", StringValue(peakRate));
  QueueDiscContainer qdiscs = tch.Install(devices.Get(0));

  // Enable Tracing
  if (enablePcap) {
    pointToPoint.EnablePcapAll("tbf-example");
  }

  // Token Bucket Tracing
  Simulator::Schedule(Seconds(0.1), [&]() {
      Ptr<TokenBucketFilter> tbf = DynamicCast<TokenBucketFilter>(qdiscs.Get(0));
      if (tbf) {
          NS_LOG_UNCOND("Time: " << Simulator::Now().GetSeconds()
                        << ", First Bucket Tokens: " << tbf->GetFirstBucketTokens()
                        << ", Second Bucket Tokens: " << tbf->GetSecondBucketTokens());
      }
      else {
          NS_LOG_ERROR("Queue disc is not a TBF!");
      }
  });

  Simulator::Schedule(Seconds(1.0), [&]() {
      Ptr<TokenBucketFilter> tbf = DynamicCast<TokenBucketFilter>(qdiscs.Get(0));
      if (tbf) {
          NS_LOG_UNCOND("Time: " << Simulator::Now().GetSeconds()
                        << ", First Bucket Tokens: " << tbf->GetFirstBucketTokens()
                        << ", Second Bucket Tokens: " << tbf->GetSecondBucketTokens());
      }
      else {
          NS_LOG_ERROR("Queue disc is not a TBF!");
      }
  });

    Simulator::Schedule(Seconds(2.0), [&]() {
      Ptr<TokenBucketFilter> tbf = DynamicCast<TokenBucketFilter>(qdiscs.Get(0));
      if (tbf) {
          NS_LOG_UNCOND("Time: " << Simulator::Now().GetSeconds()
                        << ", First Bucket Tokens: " << tbf->GetFirstBucketTokens()
                        << ", Second Bucket Tokens: " << tbf->GetSecondBucketTokens());
      }
      else {
          NS_LOG_ERROR("Queue disc is not a TBF!");
      }
  });

  // Flow Monitor
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  // Animation Interface
  AnimationInterface anim(animFile);

  // Run the simulation
  Simulator::Stop(Seconds(10.0));
  Simulator::Run();

  // Print per flow statistics
  monitor->CheckForLostPackets();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
  for (auto i = stats.begin(); i != stats.end(); ++i) {
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
    NS_LOG_UNCOND("Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n");
    NS_LOG_UNCOND("  Tx Packets: " << i->second.txPackets);
    NS_LOG_UNCOND("  Rx Packets: " << i->second.rxPackets);
    NS_LOG_UNCOND("  Lost Packets: " << i->second.lostPackets);
    NS_LOG_UNCOND("  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1000000 << " Mbps");
  }

  // Print TBF queue statistics
  Ptr<QueueDisc> queueDisc = qdiscs.Get(0);
  if (queueDisc != nullptr) {
      NS_LOG_UNCOND("QueueDisc enqueues: " << queueDisc->GetTotalEnqueuedPackets());
      NS_LOG_UNCOND("QueueDisc dequeues: " << queueDisc->GetTotalDequeuedPackets());
      NS_LOG_UNCOND("QueueDisc drops: " << queueDisc->GetTotalDroppedPackets());
  } else {
      NS_LOG_ERROR("Queue disc is null!");
  }

  Simulator::Destroy();
  return 0;
}