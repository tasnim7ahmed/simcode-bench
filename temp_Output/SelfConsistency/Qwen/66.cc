#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/tcp-header.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpWestwoodExample");

class TcpPerformanceTracer {
public:
  TcpPerformanceTracer();
  ~TcpPerformanceTracer();

  void TraceCongestionWindow(Ptr<OutputStreamWrapper> stream, uint32_t oldVal, uint32_t newVal);
  void TraceSlowStartThreshold(Ptr<OutputStreamWrapper> stream, uint32_t oldVal, uint32_t newVal);
  void TraceRtt(Ptr<OutputStreamWrapper> stream, Time oldVal, Time newVal);
  void TraceRto(Ptr<OutputStreamWrapper> stream, Time oldVal, Time newVal);
  void TraceInFlight(Ptr<OutputStreamWrapper> stream, uint32_t oldVal, uint32_t newVal);

private:
  std::map<uint32_t, Ptr<OutputStreamWrapper>> m_cwndStreams;
  std::map<uint32_t, Ptr<OutputStreamWrapper>> m_ssthreshStreams;
  std::map<uint32_t, Ptr<OutputStreamWrapper>> m_rttStreams;
  std::map<uint32_t, Ptr<OutputStreamWrapper>> m_rtoStreams;
  std::map<uint32_t, Ptr<OutputStreamWrapper>> m_inFlightStreams;
};

TcpPerformanceTracer::TcpPerformanceTracer() {}

TcpPerformanceTracer::~TcpPerformanceTracer() {}

void
TcpPerformanceTracer::TraceCongestionWindow(Ptr<OutputStreamWrapper> stream, uint32_t oldVal, uint32_t newVal)
{
  *stream->GetStream() << Simulator::Now().GetSeconds() << "\t" << newVal << std::endl;
}

void
TcpPerformanceTracer::TraceSlowStartThreshold(Ptr<OutputStreamWrapper> stream, uint32_t oldVal, uint32_t newVal)
{
  *stream->GetStream() << Simulator::Now().GetSeconds() << "\t" << newVal << std::endl;
}

void
TcpPerformanceTracer::TraceRtt(Ptr<OutputStreamWrapper> stream, Time oldVal, Time newVal)
{
  *stream->GetStream() << Simulator::Now().GetSeconds() << "\t" << newVal.GetSeconds() << std::endl;
}

void
TcpPerformanceTracer::TraceRto(Ptr<OutputStreamWrapper> stream, Time oldVal, Time newVal)
{
  *stream->GetStream() << Simulator::Now().GetSeconds() << "\t" << newVal.GetSeconds() << std::endl;
}

void
TcpPerformanceTracer::TraceInFlight(Ptr<OutputStreamWrapper> stream, uint32_t oldVal, uint32_t newVal)
{
  *stream->GetStream() << Simulator::Now().GetSeconds() << "\t" << newVal << std::endl;
}

static void
SetTCPStack(std::string tcpVariant)
{
  Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpSocketFactory::GetTypeId()));
  if (tcpVariant == "Westwood")
    {
      Config::SetDefault("ns3::TcpSocket::SndBufSize", UintegerValue(131072));
      Config::SetDefault("ns3::TcpSocket::RcvBufSize", UintegerValue(131072));
      Config::SetDefault("ns3::TcpWestwood::FilterType", EnumValue(TcpWestwood::Tso));
      Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpWestwood::GetTypeId()));
    }
  else if (tcpVariant == "WestwoodPlus")
    {
      Config::SetDefault("ns3::TcpSocket::SndBufSize", UintegerValue(131072));
      Config::SetDefault("ns3::TcpSocket::RcvBufSize", UintegerValue(131072));
      Config::SetDefault("ns3::TcpWestwood::FilterType", EnumValue(TcpWestwood::None));
      Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpWestwood::GetTypeId()));
    }
}

int
main(int argc, char *argv[])
{
  std::string tcpVariant = "Westwood";
  std::string bandwidth = "1Mbps";
  std::string delay = "20ms";
  double errorRate = 0.0;
  uint32_t flowCount = 1;

  CommandLine cmd(__FILE__);
  cmd.AddValue("tcpVariant", "TCP variant to use: Westwood or WestwoodPlus", tcpVariant);
  cmd.AddValue("bandwidth", "Bottleneck bandwidth", bandwidth);
  cmd.AddValue("delay", "Bottleneck delay", delay);
  cmd.AddValue("errorRate", "Packet error rate", errorRate);
  cmd.AddValue("flowCount", "Number of flows", flowCount);
  cmd.Parse(argc, argv);

  SetTCPStack(tcpVariant);

  NodeContainer nodes;
  nodes.Create(2);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute("DataRate", StringValue(bandwidth));
  pointToPoint.SetChannelAttribute("Delay", StringValue(delay));

  if (errorRate > 0)
    {
      pointToPoint.SetDeviceAttribute("ReceiveErrorModel",
                                      PointerValue(CreateObject<RateErrorModel>()));
      Config::SetDefault("ns3::RateErrorModel::ErrorRate", DoubleValue(errorRate));
    }

  NetDeviceContainer devices;
  devices = pointToPoint.Install(nodes);

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  uint16_t sinkPort = 8080;
  Address sinkAddress(InetSocketAddress(interfaces.GetAddress(1), sinkPort));
  PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
  ApplicationContainer sinkApps = packetSinkHelper.Install(nodes.Get(1));
  sinkApps.Start(Seconds(0.0));
  sinkApps.Stop(Seconds(10.0));

  TcpPerformanceTracer tracer;

  for (uint32_t i = 0; i < flowCount; ++i)
    {
      Ptr<Socket> ns3TcpSocket = Socket::CreateSocket(nodes.Get(0), TcpSocketFactory::GetTypeId());

      Ptr<OutputStreamWrapper> cwndStream = CreateFileStreamWrapper("cwnd-" + std::to_string(i) + ".txt", std::ios::out);
      ns3TcpSocket->TraceConnectWithoutContext("CongestionWindow", MakeBoundCallback(&TcpPerformanceTracer::TraceCongestionWindow, &tracer, cwndStream));

      Ptr<OutputStreamWrapper> ssthreshStream = CreateFileStreamWrapper("ssthresh-" + std::to_string(i) + ".txt", std::ios::out);
      ns3TcpSocket->TraceConnectWithoutContext("SlowStartThreshold", MakeBoundCallback(&TcpPerformanceTracer::TraceSlowStartThreshold, &tracer, ssthreshStream));

      Ptr<OutputStreamWrapper> rttStream = CreateFileStreamWrapper("rtt-" + std::to_string(i) + ".txt", std::ios::out);
      ns3TcpSocket->TraceConnectWithoutContext("RTT", MakeBoundCallback(&TcpPerformanceTracer::TraceRtt, &tracer, rttStream));

      Ptr<OutputStreamWrapper> rtoStream = CreateFileStreamWrapper("rto-" + std::to_string(i) + ".txt", std::ios::out);
      ns3TcpSocket->TraceConnectWithoutContext("RTO", MakeBoundCallback(&TcpPerformanceTracer::TraceRto, &tracer, rtoStream));

      Ptr<OutputStreamWrapper> inFlightStream = CreateFileStreamWrapper("inflight-" + std::to_string(i) + ".txt", std::ios::out);
      ns3TcpSocket->TraceConnectWithoutContext("BytesInFlight", MakeBoundCallback(&TcpPerformanceTracer::TraceInFlight, &tracer, inFlightStream));

      Ptr<MyApp> app = CreateObject<MyApp>();
      app->Setup(ns3TcpSocket, sinkAddress, 1040, 1000, DataRate("500kbps"));
      nodes.Get(0)->AddApplication(app);
      app->SetStartTime(Seconds(1.0 + i * 0.1));
      app->SetStopTime(Seconds(10.0));
    }

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  Simulator::Run();
  Simulator::Destroy();
  return 0;
}

class MyApp : public Application
{
public:
  static TypeId GetTypeId(void);
  MyApp();
  virtual ~MyApp();

  void Setup(Ptr<Socket> socket, Address address, uint32_t packetSize, uint32_t nPackets, DataRate dataRate);

private:
  virtual void StartApplication(void);
  virtual void StopApplication(void);

  void SendPacket(void);

  Ptr<Socket>     m_socket;
  Address         m_peer;
  uint32_t        m_packetSize;
  uint32_t        m_nPackets;
  DataRate        m_dataRate;
  EventId         m_sendEvent;
  bool            m_running;
  uint32_t        m_packetsSent;
};

TypeId MyApp::GetTypeId(void)
{
  static TypeId tid = TypeId ("MyApp")
    .SetParent<Application> ()
    .SetGroupName("Tutorial")
    .AddConstructor<MyApp> ()
    ;
  return tid;
}

MyApp::MyApp()
  : m_socket(0),
    m_peer(),
    m_packetSize(0),
    m_nPackets(0),
    m_dataRate(),
    m_sendEvent(),
    m_running(false),
    m_packetsSent(0)
{
}

MyApp::~MyApp()
{
  m_socket = nullptr;
}

void
MyApp::Setup(Ptr<Socket> socket, Address address, uint32_t packetSize, uint32_t nPackets, DataRate dataRate)
{
  m_socket = socket;
  m_peer = address;
  m_packetSize = packetSize;
  m_nPackets = nPackets;
  m_dataRate = dataRate;
}

void
MyApp::StartApplication(void)
{
  m_running = true;
  m_packetsSent = 0;
  m_socket->Bind();
  m_socket->Connect(m_peer);
  SendPacket();
}

void
MyApp::StopApplication(void)
{
  m_running = false;

  if (m_sendEvent.IsRunning())
    {
      Simulator::Cancel(m_sendEvent);
    }

  if (m_socket)
    {
      m_socket->Close();
    }
}

void
MyApp::SendPacket(void)
{
  Ptr<Packet> packet = Create<Packet>(m_packetSize);
  m_socket->Send(packet);

  if (++m_packetsSent < m_nPackets)
    {
      Time tNext(Seconds((double)m_packetSize * 8 / (double)m_dataRate.GetBitRate()));
      m_sendEvent = Simulator::Schedule(tNext, &MyApp::SendPacket, this);
    }
}