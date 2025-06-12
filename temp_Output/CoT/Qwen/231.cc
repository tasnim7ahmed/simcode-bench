#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/application.h"
#include "ns3/socket.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpMinimalExample");

class TcpServer : public Application
{
public:
  TcpServer ();
  virtual ~TcpServer ();

  static TypeId GetTypeId (void);
  virtual void StartApplication (void);
  virtual void StopApplication (void);

private:
  void HandleAccept (Ptr<Socket> s, const Address& from);
  void HandleRead (Ptr<Socket> socket);

  Ptr<Socket> m_socket;
  std::vector<Ptr<Socket>> m_clientSockets;
};

TcpServer::TcpServer ()
  : m_socket (0)
{
}

TcpServer::~TcpServer ()
{
  m_clientSockets.clear ();
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
      InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), 50000);
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

  for (auto sock : m_clientSockets)
    {
      sock->Close ();
    }
  m_clientSockets.clear ();
}

void
TcpServer::HandleAccept (Ptr<Socket> s, const Address& from)
{
  NS_LOG_INFO ("TCP connection accepted from " << from);
  s->SetRecvCallback (MakeCallback (&TcpServer::HandleRead, this));
  m_clientSockets.push_back (s);
}

void
TcpServer::HandleRead (Ptr<Socket> socket)
{
  Ptr<Packet> packet;
  while ((packet = socket->Recv ()))
    {
      NS_LOG_INFO ("Received " << packet->GetSize () << " bytes");
    }
}

class TcpClient : public Application
{
public:
  TcpClient ();
  virtual ~TcpClient ();

  static TypeId GetTypeId (void);
  virtual void StartApplication (void);
  virtual void StopApplication (void);

private:
  void SendData ();

  Ptr<Socket> m_socket;
  Ipv4Address m_serverIp;
  uint16_t m_serverPort;
  bool m_sent;
};

TcpClient::TcpClient ()
  : m_socket (0), m_serverIp ("10.1.1.2"), m_serverPort (50000), m_sent (false)
{
}

TcpClient::~TcpClient ()
{
  if (m_socket)
    {
      m_socket->Close ();
    }
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
  if (!m_socket)
    {
      TypeId tid = TypeId::LookupByName ("ns3::TcpSocketFactory");
      m_socket = Socket::CreateSocket (GetNode (), tid);
      m_socket->Connect (InetSocketAddress (m_serverIp, m_serverPort));
      Simulator::Schedule (Seconds (0.1), &TcpClient::SendData, this);
    }
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
TcpClient::SendData ()
{
  if (!m_sent)
    {
      Ptr<Packet> packet = Create<Packet> (128); // 128 bytes
      m_socket->Send (packet);
      NS_LOG_INFO ("Sent 128 bytes to server");
      m_sent = true;
    }
}

int
main (int argc, char *argv[])
{
  CommandLine cmd (__FILE__);
  cmd.Parse (argc, argv);

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

  TcpServerHelper serverHelper;
  ApplicationContainer serverApps = serverHelper.Install (nodes.Get (1));
  serverApps.Start (Seconds (0.0));
  serverApps.Stop (Seconds (2.0));

  TcpClientHelper clientHelper;
  ApplicationContainer clientApps = clientHelper.Install (nodes.Get (0));
  clientApps.Start (Seconds (0.1));
  clientApps.Stop (Seconds (2.0));

  Simulator::Stop (Seconds (2.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}

class TcpServerHelper : public ObjectFactory
{
public:
  TcpServerHelper ()
  {
    SetTypeId ("TcpServer");
  }
};

class TcpClientHelper : public ObjectFactory
{
public:
  TcpClientHelper ()
  {
    SetTypeId ("TcpClient");
  }
};