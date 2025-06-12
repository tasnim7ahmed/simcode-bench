#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpExample");

int main(int argc, char* argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);
  LogComponent::SetFilter("TcpExample", LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create(2);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  pointToPoint.SetChannelAttribute("Delay", StringValue("10ms"));

  NetDeviceContainer devices;
  devices = pointToPoint.Install(nodes);

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("192.168.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  uint16_t port = 9;
  Address sinkLocalAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
  PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", sinkLocalAddress);
  ApplicationContainer sinkApp = packetSinkHelper.Install(nodes.Get(1));
  sinkApp.Start(Seconds(0.0));
  sinkApp.Stop(Seconds(11.0));

  Ptr<Socket> ns3TcpSocket =
      Socket::CreateSocket(nodes.Get(0), TcpSocketFactory::GetTypeId());

  Ptr<MyApp> app = CreateObject<MyApp>();
  app->Setup(ns3TcpSocket, InetSocketAddress(interfaces.GetAddress(1), port),
             1024, 10, Seconds(1));
  nodes.Get(0)->AddApplication(app);
  app->Start(Seconds(1.0));
  app->Stop(Seconds(11.0));

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  Simulator::Run();
  Simulator::Destroy();

  return 0;
}

class MyApp : public Application {
 public:
  MyApp();
  virtual ~MyApp();

  void Setup(Ptr<Socket> socket, Address address, uint32_t packetSize,
             uint32_t nPackets, Time interPacketInterval);

  void StartApplication() override;
  void StopApplication() override;

 private:
  void ScheduleTx();
  void SendPacket();

  Ptr<Socket> m_socket;
  Address m_peer;
  uint32_t m_packetSize;
  uint32_t m_nPackets;
  Time m_interPacketInterval;
  DataRate m_dataRate;
  EventId m_sendEvent;
  bool m_running;
  uint32_t m_packetsSent;
};

MyApp::MyApp()
    : m_socket(nullptr),
      m_peer(),
      m_packetSize(0),
      m_nPackets(0),
      m_interPacketInterval(),
      m_dataRate(0),
      m_sendEvent(),
      m_running(false),
      m_packetsSent(0) {}

MyApp::~MyApp() { m_socket = nullptr; }

void MyApp::Setup(Ptr<Socket> socket, Address address, uint32_t packetSize,
                  uint32_t nPackets, Time interPacketInterval) {
  m_socket = socket;
  m_peer = address;
  m_packetSize = packetSize;
  m_nPackets = nPackets;
  m_interPacketInterval = interPacketInterval;
}

void MyApp::StartApplication() {
  m_running = true;
  m_packetsSent = 0;
  m_socket->Bind();
  m_socket->Connect(m_peer);
  ScheduleTx();
}

void MyApp::StopApplication() {
  m_running = false;

  if (m_sendEvent.IsRunning()) {
    Simulator::Cancel(m_sendEvent);
  }

  if (m_socket) {
    m_socket->Close();
  }
}

void MyApp::SendPacket() {
  Ptr<Packet> packet = Create<Packet>(m_packetSize);
  m_socket->Send(packet);

  if (++m_packetsSent < m_nPackets) {
    ScheduleTx();
  }
}

void MyApp::ScheduleTx() {
  if (m_running) {
    Time tNext(m_interPacketInterval);
    m_sendEvent = Simulator::Schedule(tNext, &MyApp::SendPacket, this);
  }
}