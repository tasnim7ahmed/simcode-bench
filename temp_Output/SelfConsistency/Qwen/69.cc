#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("DumbbellTopologySimulation");

class DumbbellHelper {
public:
  DumbbellHelper(uint32_t nLeafNodes, std::string queueType,
                 DataRate linkRate, Time linkDelay,
                 uint32_t minTh, uint32_t maxTh, double maxP);

  NodeContainer GetLeftLeafNodes();
  NodeContainer GetRightLeafNodes();
  Ipv4InterfaceContainer GetLeftInterfaces();
  Ipv4InterfaceContainer GetRightInterfaces();

  void InstallQueues();
  void InstallApplications();

private:
  uint32_t m_nLeafNodes;
  std::string m_queueType;
  DataRate m_linkRate;
  Time m_linkDelay;
  uint32_t m_minTh;
  uint32_t m_maxTh;
  double m_maxP;

  NodeContainer m_leftLeafNodes;
  NodeContainer m_rightLeafNodes;
  NodeContainer m_routerNodes;

  NetDeviceContainer m_leftDevices;
  NetDeviceContainer m_rightDevices;
  NetDeviceContainer m_routerDevices1;
  NetDeviceContainer m_routerDevices2;

  Ipv4InterfaceContainer m_leftInterfaces;
  Ipv4InterfaceContainer m_rightInterfaces;
  Ipv4InterfaceContainer m_routerInterfaces1;
  Ipv4InterfaceContainer m_routerInterfaces2;

  InternetStackHelper m_stackHelper;
  PointToPointHelper m_p2pHelper;
};

DumbbellHelper::DumbbellHelper(uint32_t nLeafNodes, std::string queueType,
                               DataRate linkRate, Time linkDelay,
                               uint32_t minTh, uint32_t maxTh, double maxP)
  : m_nLeafNodes(nLeafNodes),
    m_queueType(queueType),
    m_linkRate(linkRate),
    m_linkDelay(linkDelay),
    m_minTh(minTh),
    m_maxTh(maxTh),
    m_maxP(maxP)
{
  m_leftLeafNodes.Create(nLeafNodes);
  m_rightLeafNodes.Create(nLeafNodes);
  m_routerNodes.Create(2);

  m_p2pHelper.SetDeviceAttribute("DataRate", DataRateValue(linkRate));
  m_p2pHelper.SetChannelAttribute("Delay", TimeValue(linkDelay));

  // Left leaf nodes to left router
  for (uint32_t i = 0; i < nLeafNodes; ++i) {
    NetDeviceContainer devices = m_p2pHelper.Install(m_leftLeafNodes.Get(i), m_routerNodes.Get(0));
    m_leftDevices.Add(devices.Get(0));
    m_routerDevices1.Add(devices.Get(1));
  }

  // Right leaf nodes to right router
  for (uint32_t i = 0; i < nLeafNodes; ++i) {
    NetDeviceContainer devices = m_p2pHelper.Install(m_rightLeafNodes.Get(i), m_routerNodes.Get(1));
    m_rightDevices.Add(devices.Get(0));
    m_routerDevices2.Add(devices.Get(1));
  }

  // Connect the two routers
  NetDeviceContainer routerLink = m_p2pHelper.Install(m_routerNodes);
  m_routerDevices1.Add(routerLink.Get(0));
  m_routerDevices2.Add(routerLink.Get(1));

  m_stackHelper.InstallAll();

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  m_leftInterfaces = address.Assign(m_leftDevices);
  address.NewNetwork();
  m_routerInterfaces1 = address.Assign(m_routerDevices1);

  address.NewNetwork();
  m_rightInterfaces = address.Assign(m_rightDevices);
  address.NewNetwork();
  m_routerInterfaces2 = address.Assign(m_routerDevices2);
}

void DumbbellHelper::InstallQueues() {
  TrafficControlHelper tch;
  if (m_queueType == "RED") {
    tch.SetRootQueueDisc("ns3::RedQueueDisc",
                         "MinTh", DoubleValue(m_minTh),
                         "MaxTh", DoubleValue(m_maxTh),
                         "MaxP", DoubleValue(m_maxP));
  } else if (m_queueType == "AdaptiveRED") {
    tch.SetRootQueueDisc("ns3::FengAdaptiveRedQueueDisc",
                         "MinTh", DoubleValue(m_minTh),
                         "MaxTh", DoubleValue(m_maxTh),
                         "MaxP", DoubleValue(m_maxP));
  } else {
    NS_ABORT_MSG("Unknown queue type: " << m_queueType);
  }

  // Install queue discs on all leaf node devices and router links
  QueueDiscContainer qdiscs;
  for (uint32_t i = 0; i < m_nLeafNodes; ++i) {
    qdiscs = tch.Install(m_leftLeafNodes.Get(i)->GetDevice(0));
    qdiscs = tch.Install(m_rightLeafNodes.Get(i)->GetDevice(0));
  }
  // Apply queue disc on router links
  qdiscs = tch.Install(m_routerNodes.Get(0)->GetDevice(1)); // middle link
  qdiscs = tch.Install(m_routerNodes.Get(1)->GetDevice(1)); // middle link
}

void DumbbellHelper::InstallApplications() {
  uint16_t port = 5000;
  for (uint32_t i = 0; i < m_nLeafNodes; ++i) {
    InetSocketAddress sinkAddr(m_rightInterfaces.GetAddress(i), port);
    PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", sinkAddr);
    ApplicationContainer sinkApp = sinkHelper.Install(m_rightLeafNodes.Get(i));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(10.0));

    OnOffHelper onoff("ns3::TcpSocketFactory", Address());
    onoff.SetAttribute("Remote", AddressValue(sinkAddr));
    onoff.SetAttribute("OnTime", StringValue("ns3::UniformRandomVariable[Min=0.01|Max=0.1]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::UniformRandomVariable[Min=0.005|Max=0.05]"));

    ApplicationContainer app = onoff.Install(m_leftLeafNodes.Get(i));
    app.Start(Seconds(1.0));
    app.Stop(Seconds(9.0));
  }
}

NodeContainer DumbbellHelper::GetLeftLeafNodes() { return m_leftLeafNodes; }
NodeContainer DumbbellHelper::GetRightLeafNodes() { return m_rightLeafNodes; }
Ipv4InterfaceContainer DumbbellHelper::GetLeftInterfaces() { return m_leftInterfaces; }
Ipv4InterfaceContainer DumbbellHelper::GetRightInterfaces() { return m_rightInterfaces; }

int main(int argc, char *argv[]) {
  uint32_t nLeafNodes = 2;
  std::string queueType = "RED";
  DataRate linkRate("1Mbps");
  Time linkDelay = MilliSeconds(20);
  uint32_t minTh = 5;
  uint32_t maxTh = 50;
  double maxP = 0.01;
  uint32_t packetSize = 512;

  CommandLine cmd(__FILE__);
  cmd.AddValue("nLeafNodes", "Number of leaf nodes per side", nLeafNodes);
  cmd.AddValue("queueType", "Queue disc type: RED or AdaptiveRED", queueType);
  cmd.AddValue("linkRate", "Link data rate in bps", linkRate);
  cmd.AddValue("linkDelay", "Link delay", linkDelay);
  cmd.AddValue("minTh", "RED minimum threshold", minTh);
  cmd.AddValue("maxTh", "RED maximum threshold", maxTh);
  cmd.AddValue("maxP", "RED mark probability", maxP);
  cmd.AddValue("packetSize", "Packet size in bytes", packetSize);
  cmd.Parse(argc, argv);

  Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(packetSize));

  DumbbellHelper dumbbell(nLeafNodes, queueType, linkRate, linkDelay, minTh, maxTh, maxP);
  dumbbell.InstallQueues();
  dumbbell.InstallApplications();

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();

  monitor->CheckForLostPackets();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator iter = stats.begin(); iter != stats.end(); ++iter) {
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(iter->first);
    std::cout << "Flow " << iter->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
    std::cout << "  Tx Packets: " << iter->second.txPackets << "\n";
    std::cout << "  Rx Packets: " << iter->second.rxPackets << "\n";
    std::cout << "  Lost Packets: " << iter->second.lostPackets << "\n";
    std::cout << "  Throughput: " << iter->second.rxBytes * 8.0 / (iter->second.timeLastRxPacket.GetSeconds() - iter->second.timeFirstTxPacket.GetSeconds()) / 1000 << " Kbps\n";
  }

  // Print queue statistics
  for (uint32_t i = 0; i < nLeafNodes; ++i) {
    Ptr<NetDevice> dev = dumbbell.GetLeftLeafNodes().Get(i)->GetDevice(0);
    Ptr<QueueDisc> qd = dev->GetObject<QueueDisc>();
    if (qd) {
      std::cout << "Left Leaf Node " << i << " Queue Disc Stats:\n";
      qd->GetStats().Print(std::cout);
    }
  }

  Simulator::Destroy();
  return 0;
}