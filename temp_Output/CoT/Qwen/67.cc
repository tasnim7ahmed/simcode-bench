#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/flow-monitor-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("DumbbellTrafficControlExperiment");

class DumbbellTopologyHelper {
public:
  DumbbellTopologyHelper();
  void Setup(const std::string& queueDiscType);
  NodeContainer GetLeftNodes() const { return m_leftNodes; }
  NodeContainer GetRightNodes() const { return m_rightNodes; }
  Ipv4InterfaceContainer GetLeftInterfaces() const { return m_leftIfaces; }
  Ipv4InterfaceContainer GetRightInterfaces() const { return m_rightIfaces; }

private:
  NodeContainer m_leftNodes;
  NodeContainer m_rightNodes;
  NodeContainer m_routers;
  PointToPointHelper m_p2p;
  InternetStackHelper m_internet;
  TrafficControlHelper m_queueDiscHelper;
  Ipv4AddressHelper m_ipv4;
  Ipv4InterfaceContainer m_leftIfaces;
  Ipv4InterfaceContainer m_rightIfaces;
};

DumbbellTopologyHelper::DumbbellTopologyHelper()
{
  m_leftNodes.Create(7);
  m_rightNodes.Create(1);
  m_routers.Create(2);

  m_p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
  m_p2p.SetChannelAttribute("Delay", StringValue("2ms"));

  m_internet.Install(m_leftNodes);
  m_internet.Install(m_rightNodes);
  m_internet.Install(m_routers);
}

void
DumbbellTopologyHelper::Setup(const std::string& queueDiscType)
{
  NetDeviceContainer leftRouterDevices;
  NetDeviceContainer rightRouterDevices;

  // Connect left nodes to left router
  for (uint32_t i = 0; i < m_leftNodes.GetN(); ++i)
    {
      PointToPointHelper p2p;
      p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
      p2p.SetChannelAttribute("Delay", StringValue("2ms"));

      NetDeviceContainer devices = p2p.Install(m_leftNodes.Get(i), m_routers.Get(0));
      leftRouterDevices.Add(devices.Get(1));
    }

  // Connect right node to right router
  NetDeviceContainer receiverDevices = m_p2p.Install(m_routers.Get(1), m_rightNodes.Get(0));
  rightRouterDevices.Add(receiverDevices.Get(0));

  // Connect routers
  NetDeviceContainer routerDevices = m_p2p.Install(m_routers.Get(0), m_routers.Get(1));
  leftRouterDevices.Add(routerDevices.Get(0));
  rightRouterDevices.Add(routerDevices.Get(1));

  // Set queue disc on the right router's outgoing interface
  m_queueDiscHelper.SetRootQueueDisc("ns3::" + queueDiscType);
  QueueDiscContainer qdiscs = m_queueDiscHelper.Install(rightRouterDevices);
  NS_LOG_INFO("Installed queue disc: " << queueDiscType);

  // Assign IP addresses
  m_ipv4.SetBase("10.1.1.0", "255.255.255.0");
  for (uint32_t i = 0; i < m_leftNodes.GetN(); ++i)
    {
      Ipv4InterfaceContainer ifaces = m_ipv4.Assign(NetDeviceContainer(m_leftNodes.Get(i)->GetDevice(0), m_routers.Get(0)->GetDevice(i + 1)));
      m_leftIfaces.Add(ifaces);
    }

  m_ipv4.SetBase("10.2.2.0", "255.255.255.0");
  Ipv4InterfaceContainer routerIfaces = m_ipv4.Assign(routerDevices);
  m_ipv4.NewNetwork();
  Ipv4InterfaceContainer receiverIfaces = m_ipv4.Assign(receiverDevices);
  m_rightIfaces = receiverIfaces;
}

void
CwndChange(std::ofstream *os, uint32_t oldVal, uint32_t newVal)
{
  *os << Simulator::Now().GetSeconds() << " " << newVal << std::endl;
}

void
TraceQueueSize(QueueDiscContainer queueDiscs, const std::string& fileName)
{
  std::ofstream os(fileName.c_str());
  for (uint32_t i = 0; i < queueDiscs.GetN(); ++i)
    {
      Ptr<QueueDisc> qd = queueDiscs.Get(i);
      qd->TraceConnectWithoutContext("PacketsInQueue", MakeBoundCallback(&Print<uint32_t>, &os, "%f %u"));
    }
}

void
RunSimulation(const std::string& queueDiscType, double stopTime)
{
  DumbbellTopologyHelper topology;
  topology.Setup(queueDiscType);

  // Install TCP and UDP applications
  uint16_t port = 9;

  // TCP sink
  PacketSinkHelper tcpSink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
  ApplicationContainer tcpSinkApp = tcpSink.Install(topology.GetRightNodes().Get(0));
  tcpSinkApp.Start(Seconds(0.0));
  tcpSinkApp.Stop(Seconds(stopTime - 0.1));

  // UDP sink
  PacketSinkHelper udpSink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port + 1));
  ApplicationContainer udpSinkApp = udpSink.Install(topology.GetRightNodes().Get(0));
  udpSinkApp.Start(Seconds(0.0));
  udpSinkApp.Stop(Seconds(stopTime - 0.1));

  // TCP sources
  for (uint32_t i = 0; i < topology.GetLeftNodes().GetN(); ++i)
    {
      InetSocketAddress destAddr(topology.GetRightInterfaces().GetAddress(0), port);
      destAddr.SetTos(0xb8); // EF PHB

      BulkSendHelper tcpSource("ns3::TcpSocketFactory", destAddr);
      tcpSource.SetAttribute("MaxBytes", UintegerValue(0));
      ApplicationContainer tcpApp = tcpSource.Install(topology.GetLeftNodes().Get(i));
      tcpApp.Start(Seconds(1.0));
      tcpApp.Stop(Seconds(stopTime - 0.2));

      Config::ConnectWithoutContext("/NodeList/" + std::to_string(i) + "/ApplicationList/0/$ns3::BulkSendApplication/Socket/$ns3::TcpSocketBase/CongestionWindow",
                                    MakeBoundCallback(&CwndChange, new std::ofstream("cwnd-" + queueDiscType + "-" + std::to_string(i) + ".tr")));
    }

  // UDP sources
  OnOffHelper udpSource("ns3::UdpSocketFactory", InetSocketAddress(topology.GetRightInterfaces().GetAddress(0), port + 1));
  udpSource.SetConstantRate(DataRate("5Mbps"));
  ApplicationContainer udpApp = udpSource.Install(topology.GetLeftNodes().Get(0));
  udpApp.Start(Seconds(1.0));
  udpApp.Stop(Seconds(stopTime - 0.2));

  // Enable routing
  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  // Trace queue sizes
  std::ostringstream oss;
  oss << "queue-size-" << queueDiscType << ".tr";
  TraceQueueSize(topology.m_queueDiscHelper.GetQueueDiscs(), oss.str());

  Simulator::Stop(Seconds(stopTime));
  Simulator::Run();
  Simulator::Destroy();
}

int
main(int argc, char *argv[])
{
  CommandLine cmd(__FILE__);
  double stopTime = 10.0;
  cmd.AddValue("stopTime", "Simulation stop time", stopTime);
  cmd.Parse(argc, argv);

  NS_LOG_INFO("Running simulation with COBALT...");
  RunSimulation("CobaltQueueDisc", stopTime);

  NS_LOG_INFO("Running simulation with CoDel...");
  RunSimulation("CoDelQueueDisc", stopTime);

  return 0;
}