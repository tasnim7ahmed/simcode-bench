#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("AdhocUdpSimulation");

class UdpServer : public Application
{
public:
  UdpServer();
  virtual ~UdpServer();

  static TypeId GetTypeId(void);
  void Setup(Ptr<Socket> socket, uint16_t port);

private:
  virtual void StartApplication(void);
  virtual void StopApplication(void);
  void HandleRead(Ptr<Socket> socket);

  Ptr<Socket> m_socket;
  uint16_t m_port;
};

UdpServer::UdpServer()
    : m_socket(0), m_port(0)
{
}

UdpServer::~UdpServer()
{
  m_socket = nullptr;
}

TypeId UdpServer::GetTypeId(void)
{
  static TypeId tid = TypeId("UdpServer")
                          .SetParent<Application>()
                          .AddConstructor<UdpServer>();
  return tid;
}

void UdpServer::Setup(Ptr<Socket> socket, uint16_t port)
{
  m_socket = socket;
  m_port = port;
}

void UdpServer::StartApplication(void)
{
  if (!m_socket)
  {
    TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
    m_socket = Socket::CreateSocket(GetNode(), tid);
    InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), m_port);
    m_socket->Bind(local);
  }

  m_socket->SetRecvCallback(MakeCallback(&UdpServer::HandleRead, this));
}

void UdpServer::StopApplication(void)
{
  if (m_socket)
  {
    m_socket->Close();
    m_socket = nullptr;
  }
}

void UdpServer::HandleRead(Ptr<Socket> socket)
{
  Ptr<Packet> packet;
  Address from;
  while ((packet = socket->RecvFrom(from)))
  {
    NS_LOG_INFO("Received packet of size " << packet->GetSize() << " at " << Simulator::Now().As(Time::S));
  }
}

class UdpClient : public Application
{
public:
  UdpClient();
  virtual ~UdpClient();

  static TypeId GetTypeId(void);
  void Setup(Ptr<Socket> socket, Address address, uint32_t packetSize, Time interval);

private:
  virtual void StartApplication(void);
  virtual void StopApplication(void);
  void SendPacket(void);

  Ptr<Socket> m_socket;
  Address m_peerAddress;
  uint32_t m_packetSize;
  Time m_interval;
  EventId m_sendEvent;
  bool m_running;
};

UdpClient::UdpClient()
    : m_socket(0),
      m_peerAddress(),
      m_packetSize(512),
      m_interval(Seconds(1)),
      m_sendEvent(),
      m_running(false)
{
}

UdpClient::~UdpClient()
{
  m_socket = nullptr;
}

TypeId UdpClient::GetTypeId(void)
{
  static TypeId tid = TypeId("UdpClient")
                          .SetParent<Application>()
                          .AddConstructor<UdpClient>();
  return tid;
}

void UdpClient::Setup(Ptr<Socket> socket, Address address, uint32_t packetSize, Time interval)
{
  m_socket = socket;
  m_peerAddress = address;
  m_packetSize = packetSize;
  m_interval = interval;
}

void UdpClient::StartApplication(void)
{
  m_running = true;
  SendPacket();
}

void UdpClient::StopApplication(void)
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

void UdpClient::SendPacket(void)
{
  Ptr<Packet> packet = Create<Packet>(m_packetSize);
  m_socket->SendTo(packet, 0, m_peerAddress);

  NS_LOG_INFO("Sent packet of size " << m_packetSize << " at " << Simulator::Now().As(Time::S));

  if (m_running)
  {
    m_sendEvent = Simulator::Schedule(m_interval, &UdpClient::SendPacket, this);
  }
}

int main(int argc, char *argv[])
{
  CommandLine cmd(__FILE__);
  cmd.Parse(argc, argv);

  NodeContainer nodes;
  nodes.Create(2);

  YansWifiPhyHelper wifiPhy;
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  wifiPhy.SetChannel(channel.Create());

  WifiMacHelper wifiMac;
  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211a);
  wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                               "DataMode", StringValue("OfdmRate6Mbps"),
                               "ControlMode", StringValue("OfdmRate6Mbps"));

  wifiMac.SetType("ns3::AdhocWifiMac");
  NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(5.0),
                                "DeltaY", DoubleValue(0.0),
                                "GridWidth", UintegerValue(2),
                                "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(nodes);

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");

  // Server setup
  Ptr<Socket> serverSocket = Socket::CreateSocket(nodes.Get(1), tid);
  InetSocketAddress local = InetSocketAddress(interfaces.GetAddress(1), 9);
  serverSocket->Bind(local);
  serverSocket->SetRecvCallback(MakeCallback(&UdpServer::HandleRead, &UdpServer()));

  // Client setup
  Ptr<Socket> clientSocket = Socket::CreateSocket(nodes.Get(0), tid);
  InetSocketAddress remote(interfaces.GetAddress(1), 9);
  clientSocket->Connect(remote);

  UdpClient *clientApp = new UdpClient();
  clientApp->Setup(clientSocket, remote, 512, Seconds(1));
  clientApp->SetStartTime(Seconds(0.0));
  clientApp->SetStopTime(Seconds(10.0));
  nodes.Get(0)->AddApplication(clientApp);

  UdpServer *serverApp = new UdpServer();
  serverApp->Setup(serverSocket, 9);
  serverApp->SetStartTime(Seconds(0.0));
  serverApp->SetStopTime(Seconds(10.0));
  nodes.Get(1)->AddApplication(serverApp);

  Simulator::Run();
  Simulator::Destroy();
  return 0;
}