#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/udp-socket-factory.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("CustomUdpSocketExample");

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
    : m_socket(nullptr), m_port(9)
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
    NS_LOG_INFO("Received packet of size " << packet->GetSize() << " from " << from);
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
    : m_socket(nullptr), m_peerAddress(), m_packetSize(1024), m_interval(Seconds(1)), m_running(false)
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
}

void UdpClient::SendPacket(void)
{
  Ptr<Packet> packet = Create<Packet>(m_packetSize);
  m_socket->SendTo(packet, 0, m_peerAddress);

  if (m_running)
  {
    m_sendEvent = Simulator::Schedule(m_interval, &UdpClient::SendPacket, this);
  }
}

int main(int argc, char *argv[])
{
  Config::SetDefault("ns3::Ipv4GlobalRouting::RespondToInterfaceEvents", BooleanValue(true));

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

  uint16_t serverPort = 8080;

  // Server socket
  TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
  Ptr<Socket> serverSocket = Socket::CreateSocket(nodes.Get(1), tid);
  InetSocketAddress serverAddr = InetSocketAddress(interfaces.GetAddress(1), serverPort);
  serverSocket->Bind(serverAddr);

  Ptr<UdpServer> serverApp = CreateObject<UdpServer>();
  serverApp->Setup(serverSocket, serverPort);
  nodes.Get(1)->AddApplication(serverApp);
  serverApp->SetStartTime(Seconds(0.0));
  serverApp->SetStopTime(Seconds(10.0));

  // Client socket
  Ptr<Socket> clientSocket = Socket::CreateSocket(nodes.Get(0), tid);
  InetSocketAddress clientAddr = InetSocketAddress(interfaces.GetAddress(1), serverPort);

  Ptr<UdpClient> clientApp = CreateObject<UdpClient>();
  clientApp->Setup(clientSocket, clientAddr, 512, Seconds(1.0));
  nodes.Get(0)->AddApplication(clientApp);
  clientApp->SetStartTime(Seconds(0.0));
  clientApp->SetStopTime(Seconds(10.0));

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}