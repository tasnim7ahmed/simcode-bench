#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  NodeContainer nodes;
  nodes.Create(2);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

  NetDeviceContainer devices;
  devices = pointToPoint.Install(nodes);

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");

  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  uint16_t port = 50000;
  Address sinkAddress(InetSocketAddress(interfaces.GetAddress(1), port));
  PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
  ApplicationContainer sinkApps = packetSinkHelper.Install(nodes.Get(1));
  sinkApps.Start(Seconds(0.0));
  sinkApps.Stop(Seconds(10.0));

  Ptr<Socket> ns3TcpSocket = Socket::CreateSocket(nodes.Get(0), TcpSocketFactory::GetTypeId());

  Ptr<MyApp> clientApp = CreateObject<MyApp>();
  clientApp->Setup(ns3TcpSocket, sinkAddress, 1024, 1000, DataRate("1Mbps"));
  nodes.Get(0)->AddApplication(clientApp);
  clientApp->SetStartTime(Seconds(1.0));
  clientApp->SetStopTime(Seconds(10.0));

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}

class MyApp : public Application {
public:
  MyApp();
  virtual ~MyApp();

  void Setup(Ptr<Socket> socket, Address address, uint32_t packetSize, uint32_t numPackets, DataRate dataRate);

private:
  virtual void StartApplication(void);
  virtual void StopApplication(void);

  void ScheduleTx(void);
  void SendPacket(void);

  Ptr<Socket>     m_socket;
  Address          m_peer;
  uint32_t         m_packetSize;
  uint32_t         m_numPackets;
  DataRate         m_dataRate;
  EventId          m_sendEvent;
  bool             m_running;
  uint32_t         m_packetsSent;
};

MyApp::MyApp()
  : m_socket(nullptr),
    m_peer(),
    m_packetSize(0),
    m_numPackets(0),
    m_dataRate(0),
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
MyApp::Setup(Ptr<Socket> socket, Address address, uint32_t packetSize, uint32_t numPackets, DataRate dataRate)
{
  m_socket = socket;
  m_peer = address;
  m_packetSize = packetSize;
  m_numPackets = numPackets;
  m_dataRate = dataRate;
}

void
MyApp::StartApplication(void)
{
  m_running = true;
  m_packetsSent = 0;
  m_socket->Bind();
  m_socket->Connect(m_peer);
  ScheduleTx();
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

  if (++m_packetsSent < m_numPackets)
  {
    ScheduleTx();
  }
}

void
MyApp::ScheduleTx(void)
{
  if (m_running)
  {
    Time tNext(Seconds(m_packetSize * 8 / (double)m_dataRate.GetBitRate()));
    m_sendEvent = Simulator::Schedule(tNext, &MyApp::SendPacket, this);
  }
}