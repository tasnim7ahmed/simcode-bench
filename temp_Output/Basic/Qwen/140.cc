#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"

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
  void HandleRead(Ptr<Socket> socket);

  Ptr<Socket> m_socket;
  uint16_t m_port;
};

TcpServer::TcpServer()
  : m_socket(0),
    m_port(9)
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
    .AddConstructor<TcpServer>()
    ;
  return tid;
}

void TcpServer::StartApplication(void)
{
  if (!m_socket)
  {
    TypeId tid = TypeId::LookupByName("ns3::TcpSocketFactory");
    m_socket = Socket::CreateSocket(GetNode(), tid);
    InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), m_port);
    if (m_socket->Bind(local) == -1)
    {
      NS_FATAL_ERROR("Failed to bind socket");
    }
    m_socket->Listen();
    m_socket->SetAcceptCallback(
      MakeCallback(&TcpServer::HandleAccept, this),
      MakeCallback(&TcpServer::HandleAccept, this));
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
  s->SetRecvCallback(MakeCallback(&TcpServer::HandleRead, this));
}

void TcpServer::HandleRead(Ptr<Socket> socket)
{
  Ptr<Packet> packet;
  Address from;
  while ((packet = socket->RecvFrom(from)))
  {
    if (packet->GetSize() > 0)
    {
      NS_LOG_INFO("Received packet of size " << packet->GetSize() << " from " << from);
    }
  }
}

class TcpClient : public Application
{
public:
  TcpClient();
  virtual ~TcpClient();

  static TypeId GetTypeId(void);
  void SendData(void);

private:
  virtual void StartApplication(void);
  virtual void StopApplication(void);

  Ptr<Socket> m_socket;
  Address m_serverAddress;
  EventId m_sendEvent;
  bool m_running;
};

TcpClient::TcpClient()
  : m_socket(0),
    m_running(false)
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
    .AddConstructor<TcpClient>()
    ;
  return tid;
}

void TcpClient::StartApplication(void)
{
  m_running = true;
  if (!m_socket)
  {
    TypeId tid = TypeId::LookupByName("ns3::TcpSocketFactory");
    m_socket = Socket::CreateSocket(GetNode(), tid);
    m_socket->Connect(m_serverAddress);
    m_sendEvent = Simulator::Schedule(Seconds(1.0), &TcpClient::SendData, this);
  }
}

void TcpClient::StopApplication(void)
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

void TcpClient::SendData(void)
{
  if (m_running && m_socket)
  {
    Ptr<Packet> packet = Create<Packet>(1024); // 1024 bytes
    NS_LOG_INFO("Sending packet of size " << packet->GetSize());
    m_socket->Send(packet);
    m_sendEvent = Simulator::Schedule(Seconds(1.0), &TcpClient::SendData, this);
  }
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

  TcpServerHelper serverHelper;
  ApplicationContainer serverApps = serverHelper.Install(nodes.Get(1));
  serverApps.Start(Seconds(0.0));
  serverApps.Stop(Seconds(10.0));

  TcpClientHelper clientHelper;
  clientHelper.SetRemote(Address(interfaces.GetAddress(1), 9));
  ApplicationContainer clientApps = clientHelper.Install(nodes.Get(0));
  clientApps.Start(Seconds(0.5));
  clientApps.Stop(Seconds(10.0));

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  Simulator::Run();
  Simulator::Destroy();
  return 0;
}