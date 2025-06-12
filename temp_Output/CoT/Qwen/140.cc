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
  TcpServer ();
  virtual ~TcpServer ();

  static TypeId GetTypeId (void);
  void HandleAccept (Ptr<Socket> s, const Address& from);

private:
  virtual void StartApplication (void);
  virtual void StopApplication (void);

  Ptr<Socket> m_socket;
};

TcpServer::TcpServer ()
  : m_socket (0)
{
}

TcpServer::~TcpServer ()
{
  m_socket = 0;
}

TypeId
TcpServer::GetTypeId (void)
{
  static TypeId tid = TypeId ("TcpServer")
    .SetParent<Application> ()
    .SetGroupName ("Tutorial")
    .AddConstructor<TcpServer> ()
  ;
  return tid;
}

void
TcpServer::StartApplication (void)
{
  if (!m_socket)
  {
    TypeId tid = TypeId::LookupByName ("ns3::TcpSocketFactory");
    m_socket = Socket::CreateSocket (GetNode (), tid);

    InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), 8080);
    m_socket->Bind (local);
    m_socket->Listen ();
    m_socket->SetAcceptCallback (
      MakeCallback (&TcpServer::HandleAccept, this),
      MakeCallback (&TcpServer::HandleAccept, this));
  }
}

void
TcpServer::StopApplication (void)
{
  if (m_socket)
  {
    m_socket->Close ();
    m_socket->SetRecvCallback (MakeNullCallback<void, Ptr<Socket>> ());
  }
}

void
TcpServer::HandleAccept (Ptr<Socket> s, const Address& from)
{
  NS_LOG_INFO ("Server accepted connection from " << from);
  s->SetRecvCallback (MakeCallback ([](Ptr<Socket> socket)
  {
    Ptr<Packet> packet;
    while ((packet = socket->Recv ()))
    {
      NS_LOG_INFO ("Server received packet of size " << packet->GetSize ());
    }
  }));
}

class TcpClient : public Application
{
public:
  TcpClient ();
  virtual ~TcpClient ();

  static TypeId GetTypeId (void);
  void StartConnection (void);

private:
  virtual void StartApplication (void);
  virtual void StopApplication (void);

  Ptr<Socket> m_socket;
  Ipv4Address m_serverIp;
  uint16_t m_serverPort;
};

TcpClient::TcpClient ()
  : m_socket (0), m_serverIp ("10.1.1.2"), m_serverPort (8080)
{
}

TcpClient::~TcpClient ()
{
  m_socket = 0;
}

TypeId
TcpClient::GetTypeId (void)
{
  static TypeId tid = TypeId ("TcpClient")
    .SetParent<Application> ()
    .SetGroupName ("Tutorial")
    .AddConstructor<TcpClient> ()
  ;
  return tid;
}

void
TcpClient::StartApplication (void)
{
  m_socket = Socket::CreateSocket (GetNode (), TcpSocketFactory::GetTypeId ());
  m_socket->Connect (InetSocketAddress (m_serverIp, m_serverPort));
  Simulator::Schedule (Seconds (0.5), &TcpClient::StartConnection, this);
}

void
TcpClient::StopApplication (void)
{
  if (m_socket)
  {
    m_socket->Close ();
  }
}

void
TcpClient::StartConnection (void)
{
  NS_LOG_INFO ("Client sending data to server.");
  Ptr<Packet> packet = Create<Packet> (1024); // 1KB packet
  m_socket->Send (packet);
  NS_LOG_INFO ("Client sent packet of size 1024 bytes");
}

int main (int argc, char *argv[])
{
  LogComponentEnable ("TcpSocketExample", LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create (2);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer devices;
  devices = pointToPoint.Install (nodes);

  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");

  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  TcpServerHelper server ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), 8080));
  ApplicationContainer serverApp = server.Install (nodes.Get (1));
  serverApp.Start (Seconds (1.0));
  serverApp.Stop (Seconds (10.0));

  TcpClientHelper client;
  ApplicationContainer clientApp = client.Install (nodes.Get (0));
  clientApp.Start (Seconds (2.0));
  clientApp.Stop (Seconds (10.0));

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}