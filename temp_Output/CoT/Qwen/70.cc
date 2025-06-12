#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("DumbbellQueueDiscExample");

class DumbbellQueueDiscExample {
public:
  DumbbellQueueDiscExample();
  void Run(int argc, char *argv[]);
private:
  NodeContainer m_leftLeafNodes;
  NodeContainer m_rightLeafNodes;
  NodeContainer m_routers;
  NetDeviceContainer m_leftDevices;
  NetDeviceContainer m_rightDevices;
  NetDeviceContainer m_routerDevicesLeft;
  NetDeviceContainer m_routerDevicesRight;
  Ipv4InterfaceContainer m_leftInterfaces;
  Ipv4InterfaceContainer m_rightInterfaces;
  Ipv4InterfaceContainer m_routerInterfacesLeft;
  Ipv4InterfaceContainer m_routerInterfacesRight;

  uint32_t m_leafNodes;
  uint32_t m_queueLimitPackets;
  uint32_t m_queueLimitBytes;
  uint32_t m_packetSize;
  DataRate m_dataRateLeaf;
  DataRate m_dataRateRouter;
  Time m_delayLeaf;
  Time m_delayRouter;
  bool m_useByteMode;
  bool m_useRed;
  bool m_useNLRED;
  std::string m_redMinTh;
  std::string m_redMaxTh;
  double m_redMaxP;

  void CreateTopology();
  void SetupInternetStack();
  void InstallApplications();
  void SetupQueueDisc(bool useRed, bool useNLRED);
  void PrintDropStats();
};

DumbbellQueueDiscExample::DumbbellQueueDiscExample()
  : m_leafNodes(2),
    m_queueLimitPackets(100),
    m_queueLimitBytes(1500 * 100),
    m_packetSize(1000),
    m_dataRateLeaf("10Mbps"),
    m_dataRateRouter("100Mbps"),
    m_delayLeaf(MilliSeconds(2)),
    m_delayRouter(MicroSeconds(10)),
    m_useByteMode(false),
    m_useRed(false),
    m_useNLRED(false),
    m_redMinTh("5"),
    m_redMaxTh("15"),
    m_redMaxP(0.1)
{
}

void
DumbbellQueueDiscExample::Run(int argc, char *argv[])
{
  CommandLine cmd(__FILE__);
  cmd.AddValue("leafNodes", "Number of leaf nodes on each side of the dumbbell", m_leafNodes);
  cmd.AddValue("queueLimitPackets", "Queue limit in packets", m_queueLimitPackets);
  cmd.AddValue("queueLimitBytes", "Queue limit in bytes", m_queueLimitBytes);
  cmd.AddValue("packetSize", "Packet size for OnOff application", m_packetSize);
  cmd.AddValue("dataRateLeaf", "Data rate of leaf links", m_dataRateLeaf);
  cmd.AddValue("dataRateRouter", "Data rate of router link", m_dataRateRouter);
  cmd.AddValue("delayLeaf", "Delay of leaf links", m_delayLeaf);
  cmd.AddValue("delayRouter", "Delay of router link", m_delayRouter);
  cmd.AddValue("useByteMode", "Use byte-based queue limits", m_useByteMode);
  cmd.AddValue("useRed", "Use RED queue disc", m_useRed);
  cmd.AddValue("useNLRED", "Use NLRED queue disc (overrides useRed)", m_useNLRED);
  cmd.AddValue("redMinTh", "RED minimum threshold (packets or bytes)", m_redMinTh);
  cmd.AddValue("redMaxTh", "RED maximum threshold (packets or bytes)", m_redMaxTh);
  cmd.AddValue("redMaxP", "RED max drop probability", m_redMaxP);
  cmd.Parse(argc, argv);

  if (m_useNLRED) {
    m_useRed = true; // NLRED is a variant of RED
  }

  CreateTopology();
  SetupInternetStack();
  SetupQueueDisc(m_useRed, m_useNLRED);
  InstallApplications();

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();

  PrintDropStats();
  Simulator::Destroy();
}

void
DumbbellQueueDiscExample::CreateTopology()
{
  m_leftLeafNodes.Create(m_leafNodes);
  m_rightLeafNodes.Create(m_leafNodes);
  m_routers.Create(2);

  PointToPointHelper p2pLeaf;
  p2pLeaf.SetDeviceAttribute("DataRate", DataRateValue(m_dataRateLeaf));
  p2pLeaf.SetChannelAttribute("Delay", TimeValue(m_delayLeaf));

  PointToPointHelper p2pRouter;
  p2pRouter.SetDeviceAttribute("DataRate", DataRateValue(m_dataRateRouter));
  p2pRouter.SetChannelAttribute("Delay", TimeValue(m_delayRouter));

  // Connect left leaves to left router
  for (uint32_t i = 0; i < m_leafNodes; ++i)
  {
    NetDeviceContainer devs = p2pLeaf.Install(m_leftLeafNodes.Get(i), m_routers.Get(0));
    m_leftDevices.Add(devs.Get(0));
    m_routerDevicesLeft.Add(devs.Get(1));
  }

  // Connect right leaves to right router
  for (uint32_t i = 0; i < m_leafNodes; ++i)
  {
    NetDeviceContainer devs = p2pLeaf.Install(m_rightLeafNodes.Get(i), m_routers.Get(1));
    m_rightDevices.Add(devs.Get(0));
    m_routerDevicesRight.Add(devs.Get(1));
  }

  // Connect routers together
  NetDeviceContainer routerDevs = p2pRouter.Install(m_routers);
  m_routerDevicesLeft.Add(routerDevs.Get(0));
  m_routerDevicesRight.Add(routerDevs.Get(1));
}

void
DumbbellQueueDiscExample::SetupInternetStack()
{
  InternetStackHelper stack;
  stack.InstallAll();

  Ipv4AddressHelper ipv4;

  // Left leaf-router links
  for (uint32_t i = 0; i < m_leafNodes; ++i)
  {
    ipv4.SetBase("10.1." + std::to_string(i) + ".0", "255.255.255.0");
    Ipv4InterfaceContainer ifc = ipv4.Assign(m_leftDevices.Get(i));
    m_leftInterfaces.Add(ifc);
  }

  // Right leaf-router links
  for (uint32_t i = 0; i < m_leafNodes; ++i)
  {
    ipv4.SetBase("10.2." + std::to_string(i) + ".0", "255.255.255.0");
    Ipv4InterfaceContainer ifc = ipv4.Assign(m_rightDevices.Get(i));
    m_rightInterfaces.Add(ifc);
  }

  // Router-router link
  ipv4.SetBase("10.0.0.0", "255.255.255.0");
  Ipv4InterfaceContainer routerIfc = ipv4.Assign(m_routerDevicesLeft.Get(2));
  m_routerInterfacesLeft.Add(routerIfc);
  routerIfc = ipv4.Assign(m_routerDevicesRight.Get(2));
  m_routerInterfacesRight.Add(routerIfc);

  // Set up routing
  Ipv4GlobalRoutingHelper::PopulateRoutingTables();
}

void
DumbbellQueueDiscExample::SetupQueueDisc(bool useRed, bool useNLRED)
{
  TrafficControlHelper tch;
  ObjectFactory queueFactory;

  if (useRed)
  {
    if (useNLRED)
    {
      tch.SetRootQueueDisc("ns3::NlredQueueDisc",
                           "LinkBandwidth", DataRateValue(m_dataRateRouter),
                           "LinkDelay", TimeValue(m_delayRouter),
                           "ECN", BooleanValue(true));
    }
    else
    {
      tch.SetRootQueueDisc("ns3::RedQueueDisc",
                           "LinkBandwidth", DataRateValue(m_dataRateRouter),
                           "LinkDelay", TimeValue(m_delayRouter));
    }

    Config::SetDefault("ns3::RedQueueDisc::Gentle", BooleanValue(true));
    Config::SetDefault("ns3::RedQueueDisc::MinTh", StringValue(m_redMinTh));
    Config::SetDefault("ns3::RedQueueDisc::MaxTh", StringValue(m_redMaxTh));
    Config::SetDefault("ns3::RedQueueDisc::MaxP", DoubleValue(m_redMaxP));
  }
  else
  {
    tch.SetRootQueueDisc("ns3::FifoQueueDisc");
  }

  if (m_useByteMode)
  {
    queueFactory.SetTypeId("ns3::DropTailQueue<QueueDiscItem>");
    queueFactory.Set("MaxSize", QueueSizeValue(QueueSize(QueueSizeUnit::BYTES, m_queueLimitBytes)));
    tch.AddInternalQueues("ns3::DropTailQueue<QueueDiscItem>", 1, queueFactory);
  }
  else
  {
    queueFactory.SetTypeId("ns3::DropTailQueue<QueueDiscItem>");
    queueFactory.Set("MaxSize", QueueSizeValue(QueueSize(QueueSizeUnit::PACKETS, m_queueLimitPackets)));
    tch.AddInternalQueues("ns3::DropTailQueue<QueueDiscItem>", 1, queueFactory);
  }

  tch.Install(m_routerDevicesLeft.Get(2)); // Install on the router's outgoing device
}

void
DumbbellQueueDiscExample::InstallApplications()
{
  uint16_t port = 9;

  for (uint32_t i = 0; i < m_leafNodes; ++i)
  {
    Address sinkAddr(InetSocketAddress(m_routerInterfacesRight.GetAddress(0), port));
    PacketSinkHelper packetSink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApp = packetSink.Install(m_rightLeafNodes.Get(i));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(10.0));

    OnOffHelper onoff("ns3::TcpSocketFactory", Address());
    onoff.SetAttribute("Remote", AddressValue(sinkAddr));
    onoff.SetAttribute("OnTime", StringValue("ns3::ExponentialRandomVariable[Mean=1]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::ExponentialRandomVariable[Mean=0.5]"));
    onoff.SetAttribute("PacketSize", UintegerValue(m_packetSize));
    onoff.SetAttribute("DataRate", DataRateValue(DataRate("100kbps")));

    ApplicationContainer apps = onoff.Install(m_leftLeafNodes.Get(i));
    apps.Start(Seconds(1.0));
    apps.Stop(Seconds(10.0));
  }
}

void
DumbbellQueueDiscExample::PrintDropStats()
{
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();

  monitor->CheckForLostPackets();

  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();
  uint32_t totalDrops = 0;

  for (auto [flowId, flowStat] : stats)
  {
    std::cout << "Flow " << flowId << " (" 
              << Names::FindName(monitor->GetClassifier()->FindFlow(flowId).sourceAddress) 
              << " -> " 
              << Names::FindName(monitor->GetClassifier()->FindFlow(flowId).destinationAddress) 
              << "):\n";
    std::cout << "  Tx Packets: " << flowStat.txPackets << "\n";
    std::cout << "  Rx Packets: " << flowStat.rxPackets << "\n";
    std::cout << "  Lost Packets: " << flowStat.lostPackets << "\n";
    std::cout << "  Pkt Drop Rate: " << ((double)flowStat.lostPackets / flowStat.txPackets) << "\n";
    totalDrops += flowStat.lostPackets;
  }

  std::cout << "Total Drops: " << totalDrops << "\n";

  Simulator::Destroy();
}

int
main(int argc, char *argv[])
{
  DumbbellQueueDiscExample().Run(argc, argv);
  return 0;
}