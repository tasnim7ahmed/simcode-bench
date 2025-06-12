#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/tcp-socket-factory.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpSocketExample");

class TcpServer : public Application
{
public:
  TcpServer();
  virtual ~TcpServer();

  static TypeId GetTypeId(void);
  void HandleAccept(Ptr<Socket> s, const Address& from);

private:
  virtual void StartApplication(void);
  virtual void StopApplication(void);

  Ptr<Socket> m_socket;
};

TcpServer::TcpServer()
  : m_socket(nullptr)
{
}

TcpServer::~TcpServer()
{
  m_socket = nullptr;
}

TypeId TcpServer::GetTypeId(void)
{
  static TypeId tid = TypeId("TcpServer")
    .SetParent<Application>()
    .SetGroupName("Tutorial")
    .AddConstructor<TcpServer>();
  return tid;
}

void TcpServer::StartApplication(void)
{
  if (!m_socket)
  {
    TypeId tid = TypeId::LookupByName("ns3::TcpSocketFactory");
    m_socket = Socket::CreateSocket(GetNode(), tid);

    InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), 8080);
    if (m_socket->Bind(local) == -1)
    {
      NS_FATAL_ERROR("Failed to bind socket");
    }
    m_socket->Listen();
    m_socket->SetAcceptCallback(MakeCallback(&TcpServer::HandleAccept, this), MakeNullCallback<void, Ptr<Socket>, const Address&>());
  }
}

void TcpServer::StopApplication(void)
{
  if (m_socket)
  {
    m_socket->Close();
    m_socket->SetRecvCallback(MakeNullCallback<void, Ptr<Socket>>());
  }
}

void TcpServer::HandleAccept(Ptr<Socket> s, const Address& from)
{
  NS_LOG_INFO("Accepted connection from " << from);
  s->SetRecvCallback(MakeCallback([](Ptr<Socket> socket) {
    Ptr<Packet> packet = socket->Recv();
    NS_LOG_INFO("Received " << packet->GetSize() << " bytes");
  }));
}

class TcpClient : public Application
{
public:
  TcpClient();
  virtual ~TcpClient();

  static TypeId GetTypeId(void);
  void Connect();
  void SendData();

private:
  virtual void StartApplication(void);
  virtual void StopApplication(void);

  Ptr<Socket> m_socket;
  Address m_serverAddress;
};

TcpClient::TcpClient()
  : m_socket(nullptr)
{
}

TcpClient::~TcpClient()
{
  m_socket = nullptr;
}

TypeId TcpClient::GetTypeId(void)
{
  static TypeId tid = TypeId("TcpClient")
    .SetParent<Application>()
    .SetGroupName("Tutorial")
    .AddConstructor<TcpClient>();
  return tid;
}

void TcpClient::StartApplication(void)
{
  m_socket = Socket::CreateSocket(GetNode(), TcpSocketFactory::GetTypeId());
  m_serverAddress = InetSocketAddress(Ipv4Address("10.1.1.2"), 8080);
  m_socket->Connect(m_serverAddress);
  Simulator::Schedule(Seconds(1.0), &TcpClient::SendData, this);
}

void TcpClient::StopApplication(void)
{
  if (m_socket)
  {
    m_socket->Close();
  }
}

void TcpClient::SendData()
{
  Ptr<Packet> packet = Create<Packet>(1024); // 1024-byte packet
  NS_LOG_INFO("Sending packet with size " << packet->GetSize());
  m_socket->Send(packet);
}

int main(int argc, char *argv[])
{
  LogComponentEnable("TcpSocketExample", LOG_LEVEL_INFO);

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

  ApplicationContainer serverApp;
  TcpServerHelper server("ns3::TcpSocketFactory");
  serverApp = server.Install(nodes.Get(1));
  serverApp.Start(Seconds(0.0));
  serverApp.Stop(Seconds(10.0));

  ApplicationContainer clientApp;
  TcpClientHelper client("ns3::TcpSocketFactory");
  clientApp = client.Install(nodes.Get(0));
  clientApp.Start(Seconds(0.5));
  clientApp.Stop(Seconds(10.0));

  Simulator::Run();
  Simulator::Destroy();
  return 0;
}