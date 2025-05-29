#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiTcpClientServer");

int main(int argc, char* argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);
  LogComponent::SetDefaultLogLevel(LogLevel::LOG_LEVEL_INFO);
  LogComponent::SetPrintFilter(std::bind(&LogComponent::GetComponentIsLogging,
                                         std::placeholders::_1));

  NodeContainer apNode;
  apNode.Create(1);

  NodeContainer staNode;
  staNode.Create(1);

  WifiHelper wifi;
  wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
  phy.SetChannel(channel.Create());

  WifiMacHelper mac;
  Ssid ssid = Ssid("ns3-wifi");
  mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));

  NetDeviceContainer apDevice = wifi.Install(phy, mac, apNode);

  mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing",
              BooleanValue(false));
  NetDeviceContainer staDevice = wifi.Install(phy, mac, staNode);

  MobilityHelper mobility;

  mobility.SetPositionAllocator("ns3::RandomBoxPositionAllocator",
                                "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                "Z", StringValue("ns3::ConstantRandomVariable[Constant=0.0]"));
  mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel", "Bounds",
                             RectangleValue(Rectangle(0, 100, 0, 100)));
  mobility.Install(staNode);

  MobilityHelper mobilityAp;
  mobilityAp.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobilityAp.Install(apNode);

  InternetStackHelper internet;
  internet.Install(apNode);
  internet.Install(staNode);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer apInterface = address.Assign(apDevice);
  Ipv4InterfaceContainer staInterface = address.Assign(staDevice);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  uint16_t port = 9;
  Address sinkAddress(InetSocketAddress(apInterface.GetAddress(0), port));
  PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", sinkAddress);
  ApplicationContainer sinkApps = packetSinkHelper.Install(apNode.Get(0));
  sinkApps.Start(Seconds(1.0));
  sinkApps.Stop(Seconds(11.0));

  Ptr<Socket> ns3TcpSocket =
      Socket::CreateSocket(staNode.Get(0), TcpSocketFactory::GetTypeId());

  Ptr<MyApp> clientApp = CreateObject<MyApp>();
  clientApp->Setup(ns3TcpSocket, sinkAddress, 1024, 10, Seconds(1));
  staNode.Get(0)->AddApplication(clientApp);
  clientApp->SetStartTime(Seconds(2.0));
  clientApp->SetStopTime(Seconds(12.0));

  Simulator::Stop(Seconds(12.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}

class MyApp : public Application {
 public:
  MyApp();
  virtual ~MyApp();

  void Setup(Ptr<Socket> socket, Address address, uint32_t packetSize,
             uint32_t numPackets, Time packetInterval);

 private:
  virtual void StartApplication(void);
  virtual void StopApplication(void);

  void ScheduleTx(void);
  void SendPacket(void);

  Ptr<Socket> m_socket;
  Address m_peer;
  uint32_t m_packetSize;
  uint32_t m_numPackets;
  Time m_packetInterval;
  DataRate m_dataRate;
  EventId m_sendEvent;
  bool m_running;
  uint32_t m_packetsSent;
};

MyApp::MyApp()
    : m_socket(nullptr),
      m_peer(),
      m_packetSize(0),
      m_numPackets(0),
      m_packetInterval(Seconds(0)),
      m_dataRate(0),
      m_sendEvent(),
      m_running(false),
      m_packetsSent(0) {}

MyApp::~MyApp() { m_socket = nullptr; }

void MyApp::Setup(Ptr<Socket> socket, Address address, uint32_t packetSize,
                  uint32_t numPackets, Time packetInterval) {
  m_socket = socket;
  m_peer = address;
  m_packetSize = packetSize;
  m_numPackets = numPackets;
  m_packetInterval = packetInterval;
}

void MyApp::StartApplication(void) {
  m_running = true;
  m_packetsSent = 0;
  m_socket->Bind();
  m_socket->Connect(m_peer);
  SendPacket();
}

void MyApp::StopApplication(void) {
  m_running = false;

  if (m_sendEvent.IsRunning()) {
    Simulator::Cancel(m_sendEvent);
  }

  if (m_socket) {
    m_socket->Close();
  }
}

void MyApp::SendPacket(void) {
  Ptr<Packet> packet = Create<Packet>(m_packetSize);
  m_socket->Send(packet);

  if (++m_packetsSent < m_numPackets) {
    ScheduleTx();
  }
}

void MyApp::ScheduleTx(void) {
  if (m_running) {
    m_sendEvent = Simulator::Schedule(m_packetInterval, &MyApp::SendPacket, this);
  }
}