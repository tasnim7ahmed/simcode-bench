#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/flow-monitor-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("DumbbellQueueDiscExample");

class DumbbellNetwork {
public:
  DumbbellNetwork(uint32_t nLeaf, std::string queueType, uint32_t limitPackets,
                  uint32_t limitBytes, uint32_t pktSize, DataRate rate,
                  bool byteMode, Time onTime, Time offTime)
      : m_nLeaf(nLeaf), m_queueType(queueType), m_limitPackets(limitPackets),
        m_limitBytes(limitBytes), m_pktSize(pktSize), m_rate(rate),
        m_byteMode(byteMode), m_onTime(onTime), m_offTime(offTime) {}

  void Run();

private:
  uint32_t m_nLeaf;
  std::string m_queueType;
  uint32_t m_limitPackets;
  uint32_t m_limitBytes;
  uint32_t m_pktSize;
  DataRate m_rate;
  bool m_byteMode;
  Time m_onTime;
  Time m_offTime;

  NodeContainer m_leftLeafs;
  NodeContainer m_rightLeafs;
  NodeContainer m_routers;
  NetDeviceContainer m_leftDevices;
  NetDeviceContainer m_rightDevices;
  NetDeviceContainer m_routerDevices;
  Ipv4InterfaceContainer m_leftInterfaces;
  Ipv4InterfaceContainer m_rightInterfaces;
  Ipv4InterfaceContainer m_routerInterfaces;

  void SetupTopology();
  void InstallApplications();
  void ConfigureQueueDisc();
};

void DumbbellNetwork::Run() {
  SetupTopology();
  ConfigureQueueDisc();
  InstallApplications();

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();

  monitor->CheckForLostPackets();

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

  uint64_t totalDroppedPackets = 0;
  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
    if (classifier->FindFlow(i->first).destinationPort == 9) {
      totalDroppedPackets += i->second.lostPackets;
    }
  }

  NS_LOG_UNCOND("Total dropped packets: " << totalDroppedPackets);

  Simulator::Destroy();
}

void DumbbellNetwork::SetupTopology() {
  m_leftLeafs.Create(m_nLeaf);
  m_rightLeafs.Create(m_nLeaf);
  m_routers.Create(2);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", DataRateValue(DataRate("10Mbps")));
  p2p.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));

  // Left leaf to left router
  for (uint32_t i = 0; i < m_nLeaf; ++i) {
    NetDeviceContainer devs = p2p.Install(m_leftLeafs.Get(i), m_routers.Get(0));
    m_leftDevices.Add(devs);
  }

  // Right leaf to right router
  for (uint32_t i = 0; i < m_nLeaf; ++i) {
    NetDeviceContainer devs = p2p.Install(m_rightLeafs.Get(i), m_routers.Get(1));
    m_rightDevices.Add(devs);
  }

  // Connect routers
  m_routerDevices = p2p.Install(m_routers.Get(0), m_routers.Get(1));

  InternetStackHelper stack;
  stack.InstallAll();

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");

  for (uint32_t i = 0; i < m_leftDevices.GetN(); ++i) {
    m_leftInterfaces.Add(address.Assign(m_leftDevices.Get(i)));
    address.NewNetwork();
  }

  for (uint32_t i = 0; i < m_rightDevices.GetN(); ++i) {
    m_rightInterfaces.Add(address.Assign(m_rightDevices.Get(i)));
    address.NewNetwork();
  }

  m_routerInterfaces.Add(address.Assign(m_routerDevices));
}

void DumbbellNetwork::ConfigureQueueDisc() {
  TrafficControlHelper tch;
  ObjectFactory queueFactory;

  if (m_queueType == "RED") {
    queueFactory.SetTypeId("ns3::RedQueueDisc");
  } else if (m_queueType == "NLRED") {
    queueFactory.SetTypeId("ns3::NewLifoRedQueueDisc");
  } else {
    NS_FATAL_ERROR("Unknown queue type: " << m_queueType);
  }

  if (m_byteMode) {
    queueFactory.Set("MaxSize", QueueSizeValue(QueueSize("bytes", m_limitBytes)));
  } else {
    queueFactory.Set("MaxSize", QueueSizeValue(QueueSize("packets", m_limitPackets)));
  }

  // RED parameters
  queueFactory.Set("MinTh", DoubleValue(5));
  queueFactory.Set("MaxTh", DoubleValue(15));
  queueFactory.Set("LinkBandwidth", DataRateValue(m_rate));
  queueFactory.Set("LinkDelay", TimeValue(MilliSeconds(2)));

  tch.SetRootQueueDisc("ns3::PfifoFastQueueDisc");
  tch.AddInternalQueues("ns3::DropTailQueue", 1, "MaxSize", QueueSizeValue(QueueSize("packets", 1000)));

  QueueDiscContainer qdiscs = tch.Install(m_routerDevices.Get(1)); // install on right router device

  NS_LOG_INFO("Queue disc installed: " << qdiscs.Get(0));
}

void DumbbellNetwork::InstallApplications() {
  uint16_t port = 9;

  for (uint32_t i = 0; i < m_nLeaf; ++i) {
    Address sinkAddress(InetSocketAddress(m_rightInterfaces.GetAddress(i, 0), port));
    PacketSinkHelper packetSink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApp = packetSink.Install(m_rightLeafs.Get(i));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(10.0));

    OnOffHelper onoff("ns3::UdpSocketFactory", sinkAddress);
    onoff.SetConstantRate(m_rate);
    onoff.SetAttribute("OnTime", StringValue("ns3::UniformRandomVariable[Min=0.01|Max=0.1]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::UniformRandomVariable[Min=0.005|Max=0.05]"));
    onoff.SetAttribute("PacketSize", UintegerValue(m_pktSize));

    ApplicationContainer clientApp = onoff.Install(m_leftLeafs.Get(i));
    clientApp.Start(Seconds(1.0));
    clientApp.Stop(Seconds(9.0));
  }
}

int main(int argc, char *argv[]) {
  uint32_t nLeaf = 2;
  std::string queueType = "RED";
  uint32_t limitPackets = 1000;
  uint32_t limitBytes = 100000;
  uint32_t pktSize = 1024;
  std::string rateStr = "1Mbps";
  bool byteMode = false;
  std::string onTimeStr = "ns3::UniformRandomVariable[Min=0.01|Max=0.1]";
  std::string offTimeStr = "ns3::UniformRandomVariable[Min=0.005|Max=0.05]";

  CommandLine cmd(__FILE__);
  cmd.AddValue("nLeaf", "Number of left and right leaf nodes", nLeaf);
  cmd.AddValue("queueType", "Queue disc type - RED or NLRED", queueType);
  cmd.AddValue("limitPackets", "Queue limit in packets (if packet mode)", limitPackets);
  cmd.AddValue("limitBytes", "Queue limit in bytes (if byte mode)", limitBytes);
  cmd.AddValue("pktSize", "Packet size for OnOff source", pktSize);
  cmd.AddValue("rate", "Data rate for OnOff source", rateStr);
  cmd.AddValue("byteMode", "Use byte-based queue limiting", byteMode);
  cmd.AddValue("onTime", "On time random variable", onTimeStr);
  cmd.AddValue("offTime", "Off time random variable", offTimeStr);
  cmd.Parse(argc, argv);

  DataRate rate(rateStr);
  Time onTime = Seconds(0.1); // Placeholder, using uniform RV instead
  Time offTime = Seconds(0.05); // Placeholder, using uniform RV instead

  LogComponentEnable("DumbbellQueueDiscExample", LOG_LEVEL_INFO);

  DumbbellNetwork dumbbell(nLeaf, queueType, limitPackets, limitBytes, pktSize, rate, byteMode, onTime, offTime);
  dumbbell.Run();

  return 0;
}