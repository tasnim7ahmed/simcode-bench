#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/tcp-socket-factory.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpMinimalExample");

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
    InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), 50000);
    m_socket->Bind(local);
    m_socket->Listen();
    m_socket->SetAcceptCallback(MakeCallback(&TcpServer::HandleAccept, this), nullptr);
  }
}

void TcpServer::StopApplication(void)
{
  if (m_socket)
  {
    m_socket->Close();
  }
}

void TcpServer::HandleAccept(Ptr<Socket> s, const Address& from)
{
  NS_LOG_INFO("Connection from " << from);
  s->SetRecvCallback(MakeCallback(&TcpServer::HandleRead, this));
}

void TcpServer::HandleRead(Ptr<Socket> socket)
{
  Ptr<Packet> packet;
  Address from;
  while ((packet = socket->RecvFrom(from)))
  {
    NS_LOG_INFO("Received packet of size " << packet->GetSize() << " from " << from);
  }
}

int main(int argc, char *argv[])
{
  CommandLine cmd(__FILE__);
  cmd.Parse(argc, argv);

  NodeContainer nodes;
  nodes.Create(2);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute("DataRate", StringValue("1Mbps"));
  pointToPoint.SetChannelAttribute("Delay", StringValue("1ms"));

  NetDeviceContainer devices;
  devices = pointToPoint.Install(nodes.Get(0), nodes.Get(1));

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");

  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  ApplicationContainer serverApps;
  TcpServerHelper server;
  serverApps.Add(server.Install(nodes.Get(1)));
  serverApps.Start(Seconds(0.0));
  serverApps.Stop(Seconds(2.0));

  TypeId tid = TypeId::LookupByName("ns3::TcpSocketFactory");
  Ptr<Socket> clientSocket = Socket::CreateSocket(nodes.Get(0), tid);
  InetSocketAddress remote(interfaces.GetAddress(1), 50000);
  clientSocket->Connect(remote);

  Simulator::Schedule(Seconds(0.5), &Socket::Send, clientSocket, Create<Packet>(1024), 0);

  Simulator::Stop(Seconds(2.1));
  Simulator::Run();
  Simulator::Destroy();
  return 0;
}