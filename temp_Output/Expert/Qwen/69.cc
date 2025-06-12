#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("DumbbellQueueSimulation");

class DumbbellTopology {
public:
  DumbbellTopology(uint32_t leafNodes, std::string queueType,
                   uint32_t minTh, uint32_t maxTh, double maxP,
                   DataRate linkRate, Time linkDelay,
                   DataRate accessRate, Time accessDelay,
                   uint32_t pktSize, uint32_t queueSize);

  void Run(Time duration);
  void OutputStats();

private:
  void SetupStack();
  void SetupLinks();
  void InstallApplications(Time duration);
  void SetupQueues();

  uint32_t m_leafNodes;
  std::string m_queueType;
  uint32_t m_minTh;
  uint32_t m_maxTh;
  double m_maxP;
  DataRate m_linkRate;
  Time m_linkDelay;
  DataRate m_accessRate;
  Time m_accessDelay;
  uint32_t m_pktSize;
  uint32_t m_queueSize;

  NodeContainer m_leftLeafs;
  NodeContainer m_rightLeafs;
  Ptr<Node> m_leftRouter;
  Ptr<Node> m_rightRouter;
  Ipv4InterfaceContainer m_leftInterfaces;
  Ipv4InterfaceContainer m_rightInterfaces;
  Ipv4InterfaceContainer m_routerInterfaces;
};

DumbbellTopology::DumbbellTopology(uint32_t leafNodes, std::string queueType,
                                   uint32_t minTh, uint32_t maxTh, double maxP,
                                   DataRate linkRate, Time linkDelay,
                                   DataRate accessRate, Time accessDelay,
                                   uint32_t pktSize, uint32_t queueSize)
  : m_leafNodes(leafNodes),
    m_queueType(queueType),
    m_minTh(minTh),
    m_maxTh(maxTh),
    m_maxP(maxP),
    m_linkRate(linkRate),
    m_linkDelay(linkDelay),
    m_accessRate(accessRate),
    m_accessDelay(accessDelay),
    m_pktSize(pktSize),
    m_queueSize(queueSize)
{
  m_leftLeafs.Create(m_leafNodes);
  m_rightLeafs.Create(m_leafNodes);
  m_leftRouter = CreateObject<Node>();
  m_rightRouter = CreateObject<Node>();
}

void
DumbbellTopology::SetupStack()
{
  InternetStackHelper stack;
  stack.InstallAll();
}

void
DumbbellTopology::SetupLinks()
{
  PointToPointHelper accessLink;
  accessLink.SetDeviceAttribute("DataRate", DataRateValue(m_accessRate));
  accessLink.SetChannelAttribute("Delay", TimeValue(m_accessDelay));

  PointToPointHelper bottleneckLink;
  bottleneckLink.SetDeviceAttribute("DataRate", DataRateValue(m_linkRate));
  bottleneckLink.SetChannelAttribute("Delay", TimeValue(m_linkDelay));

  QueueDiscContainer qDiscs;
  if (m_queueType == "RED") {
    TrafficControlHelper tch;
    tch.SetRootQueueDisc("ns3::RedQueueDisc",
                         "MinTh", UintegerValue(m_minTh),
                         "MaxTh", UintegerValue(m_maxTh),
                         "MaxP", DoubleValue(m_maxP),
                         "QueueLimit", UintegerValue(m_queueSize));
    qDiscs = tch.Install(bottleneckLink.GetDevices().Get(0)->GetNode());
  } else if (m_queueType == "ARED") {
    TrafficControlHelper tch;
    tch.SetRootQueueDisc("ns3::RedQueueDisc",
                         "AdaptiveRed", BooleanValue(true),
                         "MinTh", UintegerValue(m_minTh),
                         "MaxTh", UintegerValue(m_maxTh),
                         "MaxP", DoubleValue(m_maxP),
                         "QueueLimit", UintegerValue(m_queueSize));
    qDiscs = tch.Install(bottleneckLink.GetDevices().Get(0)->GetNode());
  }

  Ipv4AddressHelper ip;
  ip.SetBase("10.1.1.0", "255.255.255.0");

  for (uint32_t i = 0; i < m_leafNodes; ++i) {
    NetDeviceContainer leftDevices = accessLink.Install(m_leftLeafs.Get(i), m_leftRouter);
    NetDeviceContainer rightDevices = accessLink.Install(m_rightLeafs.Get(i), m_rightRouter);

    ip.Assign(leftDevices);
    ip.Assign(rightDevices);
  }

  NetDeviceContainer routerDevices = bottleneckLink.Install(m_leftRouter, m_rightRouter);
  m_routerInterfaces = ip.Assign(routerDevices);
}

void
DumbbellTopology::InstallApplications(Time duration)
{
  uint16_t port = 50000;
  Address sinkAddress(InetSocketAddress(m_routerInterfaces.GetAddress(1), port));
  PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
  ApplicationContainer sinkApps;

  for (uint32_t i = 0; i < m_leafNodes; ++i) {
    sinkApps.Add(packetSinkHelper.Install(m_rightLeafs.Get(i)));
  }
  sinkApps.Start(Seconds(0.0));
  sinkApps.Stop(duration + Seconds(1.0));

  OnOffHelper onOffHelper("ns3::TcpSocketFactory", Address());
  onOffHelper.SetAttribute("OnTime", StringValue("ns3::UniformRandomVariable[Min=0.01|Max=0.1]"));
  onOffHelper.SetAttribute("OffTime", StringValue("ns3::UniformRandomVariable[Min=0.005|Max=0.05]"));
  onOffHelper.SetAttribute("DataRate", DataRateValue(DataRate("1000Mb/s")));
  onOffHelper.SetAttribute("PacketSize", UintegerValue(m_pktSize));

  for (uint32_t i = 0; i < m_leafNodes; ++i) {
    onOffHelper.SetAttribute("Remote", AddressValue(sinkAddress));
    ApplicationContainer apps = onOffHelper.Install(m_leftLeafs.Get(i));
    apps.Start(Seconds(1.0));
    apps.Stop(duration);
  }
}

void
DumbbellTopology::Run(Time duration)
{
  SetupStack();
  SetupLinks();
  InstallApplications(duration);

  Simulator::Stop(duration + Seconds(2.0));
  Simulator::Run();
}

void
DumbbellTopology::OutputStats()
{
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.GetMonitor();
  monitor->CheckForLostPackets();

  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();
  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator it = stats.begin(); it != stats.end(); ++it) {
    NS_LOG_UNCOND("Flow " << it->first << " packets dropped: " << it->second.packetsDropped.size());
  }

  Simulator::Destroy();
}

int
main(int argc, char *argv[])
{
  uint32_t leafNodes = 4;
  std::string queueType = "RED";
  uint32_t minTh = 5;
  uint32_t maxTh = 15;
  double maxP = 0.01;
  DataRate linkRate("1Mbps");
  Time linkDelay = MilliSeconds(200);
  DataRate accessRate("10Mbps");
  Time accessDelay = MilliSeconds(40);
  uint32_t pktSize = 1000;
  uint32_t queueSize = 20;
  Time duration = Seconds(10);

  CommandLine cmd(__FILE__);
  cmd.AddValue("leafNodes", "Number of leaf nodes per side", leafNodes);
  cmd.AddValue("queueType", "Queue type: RED or ARED", queueType);
  cmd.AddValue("minTh", "RED minimum threshold", minTh);
  cmd.AddValue("maxTh", "RED maximum threshold", maxTh);
  cmd.AddValue("maxP", "RED max drop probability", maxP);
  cmd.AddValue("linkRate", "Bottleneck link data rate", linkRate);
  cmd.AddValue("linkDelay", "Bottleneck link delay", linkDelay);
  cmd.AddValue("accessRate", "Access link data rate", accessRate);
  cmd.AddValue("accessDelay", "Access link delay", accessDelay);
  cmd.AddValue("pktSize", "Packet size in bytes", pktSize);
  cmd.AddValue("queueSize", "Queue limit in packets", queueSize);
  cmd.AddValue("duration", "Duration of the simulation in seconds", duration);
  cmd.Parse(argc, argv);

  DumbbellTopology topology(leafNodes, queueType, minTh, maxTh, maxP, linkRate, linkDelay,
                           accessRate, accessDelay, pktSize, queueSize);
  topology.Run(duration);
  topology.OutputStats();

  return 0;
}