#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("DumbbellQueueDiscExample");

class DumbbellQueueDiscExperiment {
public:
  DumbbellQueueDiscExperiment();
  void Run(uint32_t nLeaf, std::string queueType, uint32_t limitPackets, uint32_t limitBytes,
           uint32_t packetSize, DataRate dataRate, uint32_t redMinTh, uint32_t redMaxTh,
           bool useByteQueue);
  void Report();

private:
  NodeContainer leftLeafNodes;
  NodeContainer rightLeafNodes;
  NodeContainer routers;
  Ipv4InterfaceContainer leftInterfaces;
  Ipv4InterfaceContainer rightInterfaces;
  Ipv4InterfaceContainer routerInterfaces;
};

DumbbellQueueDiscExperiment::DumbbellQueueDiscExperiment() {
  // Empty constructor
}

void DumbbellQueueDiscExperiment::Run(uint32_t nLeaf, std::string queueType,
                                     uint32_t limitPackets, uint32_t limitBytes,
                                     uint32_t packetSize, DataRate dataRate,
                                     uint32_t redMinTh, uint32_t redMaxTh,
                                     bool useByteQueue) {
  // Create nodes
  leftLeafNodes.Create(nLeaf);
  rightLeafNodes.Create(nLeaf);
  routers.Create(2);

  // Create point-to-point links
  PointToPointHelper p2pRouterLink;
  p2pRouterLink.SetDeviceAttribute("DataRate", DataRateValue(dataRate));
  p2pRouterLink.SetChannelAttribute("Delay", StringValue("10ms"));

  NetDeviceContainer routerDevices;
  routerDevices = p2pRouterLink.Install(routers.Get(0), routers.Get(1));

  PointToPointHelper p2pLeafLink;
  p2pLeafLink.SetDeviceAttribute("DataRate", DataRateValue(dataRate));
  p2pLeafLink.SetChannelAttribute("Delay", StringValue("10ms"));

  NetDeviceContainer leftLeafDevices;
  NetDeviceContainer rightLeafDevices;

  for (uint32_t i = 0; i < nLeaf; ++i) {
    leftLeafDevices.Add(p2pLeafLink.Install(leftLeafNodes.Get(i), routers.Get(0)));
    rightLeafDevices.Add(p2pLeafLink.Install(rightLeafNodes.Get(i), routers.Get(1)));
  }

  // Install Internet stack
  InternetStackHelper internet;
  internet.InstallAll();

  // Assign IP addresses
  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.1.1.0", "255.255.255.0");
  routerInterfaces = ipv4.Assign(routerDevices);
  ipv4.NewNetwork();
  leftInterfaces = ipv4.Assign(leftLeafDevices);
  ipv4.NewNetwork();
  rightInterfaces = ipv4.Assign(rightLeafDevices);

  // Set up routing
  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  // Configure queue disc
  TrafficControlHelper tch;
  QueueDiscContainer qDiscs;

  if (queueType == "red") {
    tch.SetRootQueueDisc("ns3::RedQueueDisc",
                         "MaxSize", useByteQueue ? QueueSizeValue(QueueSize(QueueSizeUnit::BYTES, limitBytes)) : QueueSizeValue(QueueSize(QueueSizeUnit::PACKETS, limitPackets)),
                         "MeanPktSize", UintegerValue(packetSize),
                         "Wait", BooleanValue(true),
                         "Gentle", BooleanValue(true),
                         "MinTh", UintegerValue(redMinTh),
                         "MaxTh", UintegerValue(redMaxTh),
                         "QueueLimit", UintegerValue(limitPackets));
  } else if (queueType == "nlred") {
    tch.SetRootQueueDisc("ns3::NlredQueueDisc",
                         "MaxSize", useByteQueue ? QueueSizeValue(QueueSize(QueueSizeUnit::BYTES, limitBytes)) : QueueSizeValue(QueueSize(QueueSizeUnit::PACKETS, limitPackets)),
                         "MinTh", UintegerValue(redMinTh),
                         "MaxTh", UintegerValue(redMaxTh),
                         "QueueLimit", UintegerValue(limitPackets));
  } else {
    NS_FATAL_ERROR("Unknown queue type: " << queueType);
  }

  qDiscs = tch.Install(routerDevices.Get(1)); // install on the second router's outgoing interface

  // Install OnOff applications with random On/Off times
  ApplicationContainer apps;
  OnOffHelper onoff("ns3::UdpSocketFactory", Address());
  onoff.SetAttribute("PacketSize", UintegerValue(packetSize));
  onoff.SetAttribute("OnTime", StringValue("ns3::UniformRandomVariable[Min=0.01s|Max=0.1s]"));
  onoff.SetAttribute("OffTime", StringValue("ns3::UniformRandomVariable[Min=0.01s|Max=0.1s]"));

  for (uint32_t i = 0; i < nLeaf; ++i) {
    Ipv4Address sinkAddr = rightInterfaces.GetAddress(i);
    uint16_t port = 49 + i;
    onoff.SetAttribute("Remote", AddressValue(InetSocketAddress(sinkAddr, port)));
    apps.Add(onoff.Install(rightLeafNodes.Get(i)));
  }

  apps.Start(Seconds(0.0));
  apps.Stop(Seconds(10.0));

  // Enable flow monitor
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();

  // Get stats
  monitor->CheckForLostPackets();
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
  double totalDropped = 0;
  for (auto iter = stats.begin(); iter != stats.end(); ++iter) {
    totalDropped += iter->second.lostPackets;
  }

  NS_LOG_UNCOND("Total packets dropped in queues: " << totalDropped);

  Simulator::Destroy();
}

void DumbbellQueueDiscExperiment::Report() {
  // No additional reporting needed beyond logging
}

int main(int argc, char *argv[]) {
  uint32_t nLeaf = 2;
  std::string queueType = "red";
  uint32_t limitPackets = 100;
  uint32_t limitBytes = 100000;
  uint32_t packetSize = 1000;
  std::string dataRate = "1Mbps";
  uint32_t redMinTh = 5;
  uint32_t redMaxTh = 50;
  bool useByteQueue = false;

  CommandLine cmd(__FILE__);
  cmd.AddValue("nLeaf", "Number of leaf nodes on each side of the dumbbell", nLeaf);
  cmd.AddValue("queueType", "Queue discipline type (red or nlred)", queueType);
  cmd.AddValue("limitPackets", "Maximum number of packets in queue", limitPackets);
  cmd.AddValue("limitBytes", "Maximum number of bytes in queue", limitBytes);
  cmd.AddValue("packetSize", "Size of packets sent by applications", packetSize);
  cmd.AddValue("dataRate", "Data rate of all links", dataRate);
  cmd.AddValue("redMinTh", "RED minimum threshold (packets)", redMinTh);
  cmd.AddValue("redMaxTh", "RED maximum threshold (packets)", redMaxTh);
  cmd.AddValue("useByteQueue", "Use byte-based queue limiting instead of packet-based", useByteQueue);
  cmd.Parse(argc, argv);

  DumbbellQueueDiscExperiment experiment;
  experiment.Run(nLeaf, queueType, limitPackets, limitBytes, packetSize,
                 DataRate(dataRate), redMinTh, redMaxTh, useByteQueue);
  experiment.Report();

  return 0;
}