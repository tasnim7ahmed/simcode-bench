#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/tcp-header.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("DctcpBottleneckTopology");

class DctcpTopology : public Object
{
public:
  static TypeId GetTypeId();
  DctcpTopology();
  void Setup(const std::string& protocol);
  void ScheduleFlows();
  void StartMonitoring();
  void Report();

private:
  NodeContainer m_senders1;
  NodeContainer m_senders2;
  NodeContainer m_switches;
  NodeContainer m_receiver;
  NetDeviceContainer m_t1T2Devices;
  NetDeviceContainer m_t2R1Devices;
  Ipv4InterfaceContainer m_interfaces1;
  Ipv4InterfaceContainer m_interfaces2;
  Ipv4InterfaceContainer m_receiverInterface;
  uint16_t m_basePort;
  uint32_t m_flowCount;
  FlowMonitorHelper m_flowMonitorHelper;
  Ptr<FlowMonitor> m_flowMonitor;
};

TypeId DctcpTopology::GetTypeId()
{
  static TypeId tid = TypeId("ns3::DctcpTopology")
    .SetParent<Object>()
    .SetGroupName("Examples");
  return tid;
}

DctcpTopology::DctcpTopology()
  : m_basePort(5000),
    m_flowCount(0)
{
  m_senders1.Create(30);
  m_senders2.Create(20);
  m_switches.Create(2);
  m_receiver.Create(1);
}

void DctcpTopology::Setup(const std::string& protocol)
{
  PointToPointHelper p2p;
  p2p.SetQueue("ns3::RedQueueDisc", 
               "LinkBandwidth", DataRateValue(DataRate("10Gbps")),
               "LinkDelay", TimeValue(MicroSeconds(1)),
               "MinTh", DoubleValue(50),
               "MaxTh", DoubleValue(150),
               "ECN", BooleanValue(true));

  m_t1T2Devices = p2p.Install(m_switches.Get(0), m_switches.Get(1));

  p2p.SetDeviceAttribute("DataRate", DataRateValue(DataRate("1Gbps")));
  p2p.SetQueue("ns3::RedQueueDisc",
               "LinkBandwidth", DataRateValue(DataRate("1Gbps")),
               "LinkDelay", TimeValue(MicroSeconds(1)),
               "MinTh", DoubleValue(5),
               "MaxTh", DoubleValue(15),
               "ECN", BooleanValue(true));
  
  m_t2R1Devices = p2p.Install(m_switches.Get(1), m_receiver.Get(0));

  InternetStackHelper stack;
  stack.InstallAll();

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  m_interfaces1 = address.Assign(m_t1T2Devices);

  address.NewNetwork();
  m_interfaces2 = address.Assign(m_t2R1Devices);

  for (auto i = 0; i < m_senders1.GetN(); ++i)
    {
      PointToPointHelper senderP2P;
      senderP2P.SetDeviceAttribute("DataRate", DataRateValue(DataRate("10Gbps")));
      senderP2P.SetChannelAttribute("Delay", TimeValue(MicroSeconds(1)));
      NetDeviceContainer devices = senderP2P.Install(m_senders1.Get(i), m_switches.Get(0));
      address.NewNetwork();
      Ipv4InterfaceContainer interfaces = address.Assign(devices);
    }

  for (auto i = 0; i < m_senders2.GetN(); ++i)
    {
      PointToPointHelper senderP2P;
      senderP2P.SetDeviceAttribute("DataRate", DataRateValue(DataRate("10Gbps")));
      senderP2P.SetChannelAttribute("Delay", TimeValue(MicroSeconds(1)));
      NetDeviceContainer devices = senderP2P.Install(m_senders2.Get(i), m_switches.Get(0));
      address.NewNetwork();
      Ipv4InterfaceContainer interfaces = address.Assign(devices);
    }

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpSocketFactory::GetTypeId()));
  Config::SetDefault("ns3::TcpSocket::SndBufSize", UintegerValue(8388608));
  Config::SetDefault("ns3::TcpSocket::RcvBufSize", UintegerValue(8388608));
  Config::SetDefault("ns3::TcpSocketBase::EnableEcn", BooleanValue(true));
  Config::SetDefault("ns3::TcpSocketBase::UseEcn", EnumValue(TcpSocketBase::On));
  Config::SetDefault("ns3::TcpSocketBase::EcnMode", EnumValue(TcpSocketBase::DctcpEcnMode));

  m_flowMonitor = m_flowMonitorHelper.InstallAll();
}

void DctcpTopology::ScheduleFlows()
{
  uint32_t totalFlows = m_senders1.GetN() + m_senders2.GetN();
  double interval = 1.0 / totalFlows;

  for (uint32_t i = 0; i < m_senders1.GetN(); ++i)
    {
      InetSocketAddress sinkAddr(m_receiverInterface.GetAddress(0), m_basePort++);
      OnOffHelper onoff("ns3::TcpSocketFactory", sinkAddr);
      onoff.SetConstantRate(DataRate("10Gbps"));
      onoff.SetAttribute("PacketSize", UintegerValue(1460));
      ApplicationContainer apps = onoff.Install(m_senders1.Get(i));
      apps.Start(Seconds(i * interval));
      apps.Stop(Seconds(4.0));
      m_flowCount++;
    }

  for (uint32_t i = 0; i < m_senders2.GetN(); ++i)
    {
      InetSocketAddress sinkAddr(m_receiverInterface.GetAddress(0), m_basePort++);
      OnOffHelper onoff("ns3::TcpSocketFactory", sinkAddr);
      onoff.SetConstantRate(DataRate("10Gbps"));
      onoff.SetAttribute("PacketSize", UintegerValue(1460));
      ApplicationContainer apps = onoff.Install(m_senders2.Get(i));
      apps.Start(Seconds((m_senders1.GetN() + i) * interval));
      apps.Stop(Seconds(4.0));
      m_flowCount++;
    }

  PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), 0));
  ApplicationContainer sinkApps = sink.Install(m_receiver.Get(0));
  sinkApps.Start(Seconds(0.0));
  sinkApps.Stop(Seconds(4.0));
}

void DctcpTopology::StartMonitoring()
{
  Simulator::Schedule(Seconds(3.0), &DctcpTopology::Report, this);
}

void DctcpTopology::Report()
{
  m_flowMonitor->CheckForLostPackets();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(m_flowMonitorHelper.GetClassifier());
  std::map<FlowId, FlowStats> stats = m_flowMonitor->GetFlowStats();

  double totalRxBytes = 0;
  uint32_t flowIndex = 0;

  for (auto iter = stats.begin(); iter != stats.end(); ++iter)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(iter->first);
      if (t.destinationPort == 0) continue;

      FlowStats flow = iter->second;
      double rxBytes = static_cast<double>(flow.rxBytes);
      double duration = flow.timeLastRxPacket.GetSeconds() - flow.timeFirstTxPacket.GetSeconds();
      double throughput = (rxBytes * 8.0) / duration / 1e6;
      NS_LOG_UNCOND("Flow " << flowIndex++ << " (" << t.sourceAddress << ":" << t.sourcePort << " -> " << t.destinationAddress << ":" << t.destinationPort << ")"
                            << " Throughput: " << throughput << " Mbps");
      totalRxBytes += rxBytes;
    }

  double aggregateThroughput = (totalRxBytes * 8.0) / 1e9;
  NS_LOG_UNCOND("Aggregate Throughput: " << aggregateThroughput << " Gbps");

  QueueDiscContainer qdiscs;
  qdiscs.Add(m_switches.Get(0)->GetDevice(0)->GetObject<PointToPointNetDevice>()->GetQueueDisc());
  qdiscs.Add(m_switches.Get(1)->GetDevice(1)->GetObject<PointToPointNetDevice>()->GetQueueDisc());

  for (auto q : qdiscs)
    {
      uint32_t qlen = q->GetCurrentQueueSize().GetValue();
      double avgQlen = q->GetStats().GetNDroppedPackets(QueueDiscItem::GetTypeId());
      NS_LOG_UNCOND("Queue Stats: Current Length=" << qlen << ", Avg Drop Count=" << avgQlen);
    }
}

int main(int argc, char* argv[])
{
  std::string transportProtocol = "TcpNewReno";

  CommandLine cmd(__FILE__);
  cmd.AddValue("transportProtocol", "Transport protocol to use: TcpNewReno, TcpDctcp", transportProtocol);
  cmd.Parse(argc, argv);

  Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TypeId::LookupByName(transportProtocol)));

  DctcpTopology topology;
  topology.Setup(transportProtocol);
  topology.ScheduleFlows();
  topology.StartMonitoring();

  Simulator::Stop(Seconds(4.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}