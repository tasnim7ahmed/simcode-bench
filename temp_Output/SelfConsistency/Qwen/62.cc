#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/tcp-header.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpPacingSimulation");

class TcpMetricsTracer : public Object
{
public:
  static TypeId GetTypeId(void)
  {
    static TypeId tid = TypeId("TcpMetricsTracer")
                          .SetParent<Object>()
                          .AddConstructor<TcpMetricsTracer>();
    return tid;
  }

  void TraceCwnd(Ptr<Socket> socket, uint32_t oldVal, uint32_t newVal)
  {
    NS_LOG_INFO("Congestion Window: " << newVal);
    if (m_cwndStream)
      *m_cwndStream << Simulator::Now().GetSeconds() << " " << newVal << std::endl;
  }

  void TracePacingRate(Ptr<Socket> socket, DataRate oldVal, DataRate newVal)
  {
    NS_LOG_INFO("Pacing Rate: " << newVal.GetBitRate());
    if (m_pacingRateStream)
      *m_pacingRateStream << Simulator::Now().GetSeconds() << " " << newVal.GetBitRate() << std::endl;
  }

  void SetCwndFileStream(std::ofstream *stream)
  {
    m_cwndStream = stream;
  }

  void SetPacingRateFileStream(std::ofstream *stream)
  {
    m_pacingRateStream = stream;
  }

private:
  std::ofstream *m_cwndStream;
  std::ofstream *m_pacingRateStream;
};

int main(int argc, char *argv[])
{
  // Default simulation parameters
  double simTime = 10.0;
  bool enablePacing = true;
  uint32_t initialCwnd = 10;
  std::string transportProtocol = "TcpNewReno";

  CommandLine cmd(__FILE__);
  cmd.AddValue("simTime", "Total simulation time (seconds)", simTime);
  cmd.AddValue("enablePacing", "Enable TCP pacing", enablePacing);
  cmd.AddValue("initialCwnd", "Initial congestion window size", initialCwnd);
  cmd.AddValue("transportProtocol", "Transport protocol to use: TcpNewReno, TcpBic, TcpCubic", transportProtocol);
  cmd.Parse(argc, argv);

  Config::SetDefault("ns3::TcpSocket::SndBufSize", UintegerValue(1 << 20));
  Config::SetDefault("ns3::TcpSocket::RcvBufSize", UintegerValue(1 << 20));
  Config::SetDefault("ns3::TcpSocketBase::EnablePacing", BooleanValue(enablePacing));
  Config::SetDefault("ns3::TcpSocket::InitialCwnd", UintegerValue(initialCwnd));

  if (transportProtocol == "TcpNewReno")
  {
    Config::SetDefault("ns3::Ipv4GlobalRouting::ProtocolType", TypeIdValue(TcpNewReno::GetTypeId()));
  }
  else if (transportProtocol == "TcpBic")
  {
    Config::SetDefault("ns3::Ipv4GlobalRouting::ProtocolType", TypeIdValue(TcpBic::GetTypeId()));
  }
  else if (transportProtocol == "TcpCubic")
  {
    Config::SetDefault("ns3::Ipv4GlobalRouting::ProtocolType", TypeIdValue(TcpCubic::GetTypeId()));
  }
  else
  {
    NS_FATAL_ERROR("Unsupported transport protocol: " << transportProtocol);
  }

  // Create nodes
  NodeContainer nodes;
  nodes.Create(6);

  NodeContainer n0n1 = NodeContainer(nodes.Get(0), nodes.Get(1));
  NodeContainer n1n2 = NodeContainer(nodes.Get(1), nodes.Get(2));
  NodeContainer n2n3 = NodeContainer(nodes.Get(2), nodes.Get(3)); // Bottleneck link
  NodeContainer n3n4 = NodeContainer(nodes.Get(3), nodes.Get(4));
  NodeContainer n3n5 = NodeContainer(nodes.Get(3), nodes.Get(5));

  // Install Internet stack
  InternetStackHelper internet;
  internet.Install(nodes);

  // Configure links
  PointToPointHelper p2p;

  // High capacity upstream/downstream links
  p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("2ms"));
  NetDeviceContainer d0d1 = p2p.Install(n0n1);

  p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("5ms"));
  NetDeviceContainer d1d2 = p2p.Install(n1n2);

  // Bottleneck link
  p2p.SetDeviceAttribute("DataRate", StringValue("1.5Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("10ms"));
  NetDeviceContainer d2d3 = p2p.Install(n2n3);

  p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("2ms"));
  NetDeviceContainer d3d4 = p2p.Install(n3n4);

  NetDeviceContainer d3d5 = p2p.Install(n3n5);

  // Assign IP addresses
  Ipv4AddressHelper ipv4;

  ipv4.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i0i1 = ipv4.Assign(d0d1);

  ipv4.SetBase("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer i1i2 = ipv4.Assign(d1d2);

  ipv4.SetBase("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer i2i3 = ipv4.Assign(d2d3);

  ipv4.SetBase("10.1.4.0", "255.255.255.0");
  Ipv4InterfaceContainer i3i4 = ipv4.Assign(d3d4);

  ipv4.SetBase("10.1.5.0", "255.255.255.0");
  Ipv4InterfaceContainer i3i5 = ipv4.Assign(d3d5);

  // Setup routing
  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  // Setup applications
  uint16_t port = 50000;

  // TCP flow from n2 to n3 (bottleneck)
  Address sinkLocalAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
  PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", sinkLocalAddress);
  ApplicationContainer sinkApps = packetSinkHelper.Install(nodes.Get(4));
  sinkApps.Start(Seconds(0.0));
  sinkApps.Stop(Seconds(simTime));

  Ptr<Socket> ns3TcpSocket = Socket::CreateSocket(nodes.Get(2), TcpSocketFactory::GetTypeId());

  InetSocketAddress remote = InetSocketAddress(i3i4.GetAddress(1), port);
  Ptr<MyApp> app = CreateObject<MyApp>();
  app->Setup(ns3TcpSocket, remote, 1048576, DataRate("1Mbps"));
  nodes.Get(2)->AddApplication(app);
  app->SetStartTime(Seconds(1.0));
  app->SetStopTime(Seconds(simTime));

  // Tracing metrics
  std::ofstream cwndStream;
  cwndStream.open("cwnd-trace.txt");
  std::ofstream pacingRateStream;
  pacingRateStream.open("pacing-rate-trace.txt");

  Ptr<TcpMetricsTracer> tracer = CreateObject<TcpMetricsTracer>();
  tracer->SetCwndFileStream(&cwndStream);
  tracer->SetPacingRateFileStream(&pacingRateStream);

  ns3TcpSocket->TraceConnectWithoutContext("CongestionWindow", MakeCallback(&TcpMetricsTracer::TraceCwnd, tracer));
  ns3TcpSocket->TraceConnectWithoutContext("PacingRate", MakeCallback(&TcpMetricsTracer::TracePacingRate, tracer));

  // Flow monitor
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  Simulator::Stop(Seconds(simTime));
  Simulator::Run();

  monitor->CheckForLostPackets();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator iter = stats.begin(); iter != stats.end(); ++iter)
  {
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(iter->first);
    std::cout << "Flow ID: " << iter->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
    std::cout << "  Tx Packets: " << iter->second.txPackets << "\n";
    std::cout << "  Rx Packets: " << iter->second.rxPackets << "\n";
    std::cout << "  Throughput: " << iter->second.rxBytes * 8.0 / (iter->second.timeLastRxPacket.GetSeconds() - iter->second.timeFirstTxPacket.GetSeconds()) / 1000 / 1000 << " Mbps\n";
    std::cout << "  Mean Delay: " << iter->second.delaySum.GetSeconds() / iter->second.rxPackets << " s\n";
    std::cout << "  Jitter: " << iter->second.jitterSum.GetSeconds() / (iter->second.rxPackets > 1 ? iter->second.rxPackets - 1 : 1) << " s\n";
  }

  cwndStream.close();
  pacingRateStream.close();

  Simulator::Destroy();
  return 0;
}

class MyApp : public Application
{
public:
  static TypeId GetTypeId(void)
  {
    static TypeId tid = TypeId("MyApp")
                          .SetParent<Application>()
                          .SetGroupName("Tutorial")
                          .AddConstructor<MyApp>();
    return tid;
  }

  MyApp()
    : m_socket(0),
      m_peer(),
      m_packetSize(0),
      m_nPacketsTotal(0),
      m_packetsSent(0)
  {
  }

  ~MyApp()
  {
    m_socket = 0;
  }

  void Setup(Ptr<Socket> socket, Address address, uint32_t packetSize, DataRate dataRate)
  {
    m_socket = socket;
    m_peer = address;
    m_packetSize = packetSize;
    m_dataRate = dataRate;
  }

private:
  virtual void StartApplication(void)
  {
    m_socket->Bind();
    m_socket->Connect(m_peer);
    SendPacket();
  }

  virtual void StopApplication(void)
  {
    m_socket->Close();
  }

  void SendPacket(void)
  {
    Ptr<Packet> packet = Create<Packet>(m_packetSize);
    m_socket->Send(packet);

    if (++m_packetsSent < m_nPacketsTotal)
    {
      ScheduleTx();
    }
  }

  void ScheduleTx(void)
  {
    if (m_socket->GetTxAvailable() > m_packetSize)
    {
      Time tNext(Seconds(m_packetSize * 8.0 / m_dataRate.GetBitRate()));
      m_sendEvent = Simulator::Schedule(tNext, &MyApp::SendPacket, this);
    }
  }

  Ptr<Socket> m_socket;
  Address m_peer;
  uint32_t m_packetSize;
  uint32_t m_nPacketsTotal;
  uint32_t m_packetsSent;
  DataRate m_dataRate;
  EventId m_sendEvent;
};