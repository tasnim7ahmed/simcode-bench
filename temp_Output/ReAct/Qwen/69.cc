#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/config-store-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("DumbbellQueueSim");

class DumbbellTopology {
public:
  DumbbellTopology(uint32_t nLeaf)
    : m_nLeaf(nLeaf),
      m_queueType("RED"),
      m_redMinTh(5),
      m_redMaxTh(15),
      m_redMaxP(0.01),
      m_fengMinTh(10),
      m_fengMaxTh(20),
      m_pktSize(1000),
      m_dataRate("1Mbps"),
      m_delay("2ms"),
      m_queueSize(20),
      m_simDuration(10.0),
      m_verbose(true) {}

  void Setup();
  void InstallApplications();
  void PrintStats();

private:
  uint32_t m_nLeaf;
  std::string m_queueType;
  uint32_t m_redMinTh, m_redMaxTh;
  double m_redMaxP;
  uint32_t m_fengMinTh, m_fengMaxTh;
  uint32_t m_pktSize;
  std::string m_dataRate;
  std::string m_delay;
  uint32_t m_queueSize;
  double m_simDuration;
  bool m_verbose;

  NodeContainer m_leftRouter, m_rightRouter;
  NetDeviceContainer m_leftRouterDevices, m_rightRouterDevices, m_routerLink;
  Ipv4InterfaceContainer m_leftInterfaces, m_rightInterfaces, m_routerInterfaces;
  TcpClientHelper m_tcpClient;
};

void DumbbellTopology::Setup() {
  // Create routers and leaf nodes
  m_leftRouter.Create(1);
  m_rightRouter.Create(1);
  NodeContainer leftNodes, rightNodes;
  leftNodes.Create(m_nLeaf);
  rightNodes.Create(m_nLeaf);

  // PointToPoint links
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue(m_dataRate));
  p2p.SetChannelAttribute("Delay", StringValue(m_delay));

  // Left side links
  for (uint32_t i = 0; i < m_nLeaf; ++i) {
    NetDeviceContainer dev = p2p.Install(leftNodes.Get(i), m_leftRouter.Get(0));
    m_leftRouterDevices.Add(dev.Get(1));
  }

  // Right side links
  for (uint32_t i = 0; i < m_nLeaf; ++i) {
    NetDeviceContainer dev = p2p.Install(rightNodes.Get(i), m_rightRouter.Get(0));
    m_rightRouterDevices.Add(dev.Get(1));
  }

  // Link between routers
  m_routerLink = p2p.Install(m_leftRouter, m_rightRouter);
  
  // Internet stack
  InternetStackHelper stack;
  stack.InstallAll();

  // Assign IP addresses
  Ipv4AddressHelper ip;
  ip.SetBase("10.1.1.0", "255.255.255.0");
  m_leftInterfaces = ip.Assign(m_leftRouterDevices);
  ip.NewNetwork();
  m_rightInterfaces = ip.Assign(m_rightRouterDevices);
  ip.NewNetwork();
  m_routerInterfaces = ip.Assign(m_routerLink);

  // Traffic control: queue disc on router-router link at the left router
  TrafficControlHelper tch;
  ObjectFactory factory;

  if (m_queueType == "RED") {
    factory.SetTypeId("ns3::RedQueueDisc");
    factory.Set("MeanPktSize", UintegerValue(m_pktSize));
    factory.Set("MinTh", DoubleValue(m_redMinTh));
    factory.Set("MaxTh", DoubleValue(m_redMaxTh));
    factory.Set("MaxP", DoubleValue(m_redMaxP));
    factory.Set("QueueLimit", UintegerValue(m_queueSize));
  } else if (m_queueType == "Feng") {
    factory.SetTypeId("ns3::FengAdaptiveRedQueueDisc");
    factory.Set("MeanPktSize", UintegerValue(m_pktSize));
    factory.Set("MinTh", DoubleValue(m_fengMinTh));
    factory.Set("MaxTh", DoubleValue(m_fengMaxTh));
    factory.Set("QueueLimit", UintegerValue(m_queueSize));
  } else {
    NS_FATAL_ERROR("Unknown queue type: " << m_queueType);
  }

  tch.SetRootQueueDisc(factory);
  QueueDiscContainer qdiscs = tch.Install(m_leftRouter.Get(0)->GetDevice(1)); // router to router link device

  // Enable logging if verbose
  if (m_verbose) {
    Simulator::ScheduleNow(&QueueDisc::EnablePcapInternal, qdiscs.Get(0), "red-queue-disc-pcap", m_leftRouter.Get(0)->GetDevice(1), true, true);
  }
}

void DumbbellTopology::InstallApplications() {
  uint16_t port = 50000;
  Address sinkAddr(InetSocketAddress(m_routerInterfaces.GetAddress(1), port));

  // TCP Sink on right-side leaf nodes
  PacketSinkHelper packetSink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
  ApplicationContainer sinkApps;
  for (uint32_t i = 0; i < m_nLeaf; ++i) {
    sinkApps.Add(packetSink.Install(m_rightRouter.Get(0)));
  }
  sinkApps.Start(Seconds(0.0));
  sinkApps.Stop(Seconds(m_simDuration));

  // TCP OnOff apps on left-side nodes
  OnOffHelper client("ns3::TcpSocketFactory", sinkAddr);
  client.SetAttribute("OnTime", StringValue("ns3::UniformRandomVariable[Min=0.01|Max=0.1]"));
  client.SetAttribute("OffTime", StringValue("ns3::UniformRandomVariable[Min=0.005|Max=0.05]"));
  client.SetAttribute("PacketSize", UintegerValue(m_pktSize));
  client.SetAttribute("DataRate", DataRateValue(DataRate(m_dataRate)));

  ApplicationContainer clientApps;
  for (uint32_t i = 0; i < m_nLeaf; ++i) {
    clientApps.Add(client.Install(m_leftRouter.Get(0)));
  }
  clientApps.Start(Seconds(1.0));
  clientApps.Stop(Seconds(m_simDuration - 1));
}

void DumbbellTopology::PrintStats() {
  Ptr<QueueDisc> q = m_leftRouter.Get(0)->GetObject<TrafficControlLayer>()->GetRootQueueDisc();
  QueueDisc::Stats stats = q->GetStats();
  std::cout << "Total packets dropped: " << stats.nTotalDroppedPackets << std::endl;
  std::cout << "RED drops: " << stats.redDrops << std::endl;
  std::cout << "Feng drops: " << stats.fengDrops << std::endl;
  std::cout << "ECN marks: " << stats.ecnMarkedPackets << std::endl;
}

int main(int argc, char *argv[]) {
  uint32_t nLeaf = 2;
  std::string queueType = "RED";
  uint32_t redMinTh = 5, redMaxTh = 15;
  double redMaxP = 0.01;
  uint32_t fengMinTh = 10, fengMaxTh = 20;
  uint32_t pktSize = 1000;
  std::string dataRate = "1Mbps";
  std::string delay = "2ms";
  uint32_t queueSize = 20;
  double simDuration = 10.0;
  bool verbose = true;

  CommandLine cmd(__FILE__);
  cmd.AddValue("nLeaf", "Number of leaf nodes per side", nLeaf);
  cmd.AddValue("queueType", "Queue type (RED or Feng)", queueType);
  cmd.AddValue("redMinTh", "RED Min Threshold", redMinTh);
  cmd.AddValue("redMaxTh", "RED Max Threshold", redMaxTh);
  cmd.AddValue("redMaxP", "RED Max Marking Probability", redMaxP);
  cmd.AddValue("fengMinTh", "Feng Min Threshold", fengMinTh);
  cmd.AddValue("fengMaxTh", "Feng Max Threshold", fengMaxTh);
  cmd.AddValue("pktSize", "Packet size in bytes", pktSize);
  cmd.AddValue("dataRate", "Data rate (e.g., 1Mbps)", dataRate);
  cmd.AddValue("delay", "Link delay (e.g., 2ms)", delay);
  cmd.AddValue("queueSize", "Queue limit in packets", queueSize);
  cmd.AddValue("simDuration", "Simulation duration in seconds", simDuration);
  cmd.AddValue("verbose", "Enable verbose output", verbose);
  cmd.Parse(argc, argv);

  DumbbellTopology top(nLeaf);
  top.m_queueType = queueType;
  top.m_redMinTh = redMinTh;
  top.m_redMaxTh = redMaxTh;
  top.m_redMaxP = redMaxP;
  top.m_fengMinTh = fengMinTh;
  top.m_fengMaxTh = fengMaxTh;
  top.m_pktSize = pktSize;
  top.m_dataRate = dataRate;
  top.m_delay = delay;
  top.m_queueSize = queueSize;
  top.m_simDuration = simDuration;
  top.m_verbose = verbose;

  top.Setup();
  top.InstallApplications();

  Simulator::Stop(Seconds(simDuration));
  Simulator::Run();
  top.PrintStats();
  Simulator::Destroy();

  return 0;
}