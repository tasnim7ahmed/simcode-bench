#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/flow-monitor-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("DumbbellSimulation");

class DumbbellHelper {
public:
  DumbbellHelper();
  void SetupTopology(std::string queueDiscType);
  void InstallApplications();
  void EnableTraces();

private:
  NodeContainer m_leftLeafs;
  NodeContainer m_rightLeafs;
  NodeContainer m_routers;
  NetDeviceContainer m_leftLeafDevices;
  NetDeviceContainer m_routerLeftDevices;
  NetDeviceContainer m_routerRightDevices;
  NetDeviceContainer m_rightLeafDevices;
  Ipv4InterfaceContainer m_leftLeafInterfaces;
  Ipv4InterfaceContainer m_routerLeftInterfaces;
  Ipv4InterfaceContainer m_routerRightInterfaces;
  Ipv4InterfaceContainer m_rightLeafInterfaces;
  uint16_t m_port;
};

DumbbellHelper::DumbbellHelper()
  : m_port(50000)
{
  m_leftLeafs.Create(7);
  m_rightLeafs.Create(1);
  m_routers.Create(2);
}

void DumbbellHelper::SetupTopology(std::string queueDiscType)
{
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("10ms"));

  // Left leaf to router
  for (uint32_t i = 0; i < m_leftLeafs.GetN(); ++i)
    {
      NetDeviceContainer ndc = p2p.Install(m_leftLeafs.Get(i), m_routers.Get(0));
      m_leftLeafDevices.Add(ndc.Get(0));
      m_routerLeftDevices.Add(ndc.Get(1));
    }

  // Right leaf to router
  NetDeviceContainer rightNdc = p2p.Install(m_routers.Get(1), m_rightLeafs.Get(0));
  m_routerRightDevices.Add(rightNdc.Get(0));
  m_rightLeafDevices.Add(rightNdc.Get(1));

  // Router to router link
  Config::SetDefault("ns3::QueueDisc::Quota", UintegerValue(100));
  TrafficControlHelper tch;
  tch.SetRootQueueDisc(queueDiscType, "Interval", StringValue("100ms"), "Target", StringValue("5ms"));
  NetDeviceContainer routerLink = p2p.Install(m_routers.Get(0), m_routers.Get(1));
  QueueDiscContainer qdiscs = tch.Install(routerLink);

  InternetStackHelper internet;
  internet.InstallAll();

  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.1.1.0", "255.255.255.0");
  m_leftLeafInterfaces = ipv4.Assign(m_leftLeafDevices);
  ipv4.NewNetwork();
  m_routerLeftInterfaces = ipv4.Assign(m_routerLeftDevices);
  ipv4.NewNetwork();
  m_routerRightInterfaces = ipv4.Assign(m_routerRightDevices);
  ipv4.NewNetwork();
  m_rightLeafInterfaces = ipv4.Assign(m_rightLeafDevices);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();
}

void DumbbellHelper::InstallApplications()
{
  // TCP senders
  for (uint32_t i = 0; i < m_leftLeafs.GetN(); ++i)
    {
      Ptr<Node> sender = m_leftLeafs.Get(i);
      Ptr<Node> receiver = m_rightLeafs.Get(0);
      InetSocketAddress sinkAddr(m_rightLeafInterfaces.GetAddress(0), m_port);

      // TCP application
      OnOffHelper tcpApp("ns3::TcpSocketFactory", sinkAddr);
      tcpApp.SetConstantRate(DataRate("1Mbps"));
      ApplicationContainer tcpApps = tcpApp.Install(sender);
      tcpApps.Start(Seconds(0.0));
      tcpApps.Stop(Seconds(10.0));

      // Congestion window tracing
      std::ostringstream oss;
      oss << "cwnd-" << i << ".tr";
      AsciiTraceHelper asciiTraceHelper;
      Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream(oss.str());
      Config::ConnectWithoutContext("/NodeList/" + std::to_string(sender->GetId()) + "/$ns3::TcpL4Protocol/SocketList/0/CongestionWindow",
                                    MakeBoundCallback(&CwndChange, stream));
    }

  // UDP receiver
  PacketSinkHelper udpSink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), m_port));
  ApplicationContainer udpSinkApp = udpSink.Install(m_rightLeafs.Get(0));
  udpSinkApp.Start(Seconds(0.0));
  udpSinkApp.Stop(Seconds(10.0));
}

void DumbbellHelper::EnableTraces()
{
  // Queue size trace
  AsciiTraceHelper ascii;
  for (uint32_t i = 0; i < 2; ++i)
    {
      std::ostringstream oss;
      oss << "queue-" << i << ".tr";
      Ptr<OutputStreamWrapper> stream = ascii.CreateFileStream(oss.str());
      Config::Connect("/NodeList/" + std::to_string(m_routers.Get(0)->GetId()) +
                      "/DeviceList/1/$ns3::PointToPointNetDevice/ReceiveQueue/BytesInQueue",
                      MakeBoundCallback(&QueueSizeTrace, stream));
    }
}

void experiment(std::string queueDiscType)
{
  DumbbellHelper helper;
  helper.SetupTopology(queueDiscType);
  helper.InstallApplications();
  helper.EnableTraces();

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();
}

int main(int argc, char *argv[])
{
  CommandLine cmd(__FILE__);
  cmd.Parse(argc, argv);

  NS_LOG_INFO("Running simulation with COBALT");
  experiment("ns3::CobaltQueueDisc");

  NS_LOG_INFO("Running simulation with CoDel");
  experiment("ns3::CoDelQueueDisc");

  return 0;
}