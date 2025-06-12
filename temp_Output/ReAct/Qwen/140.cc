#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/tcp-socket-factory.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpSocketSimulation");

class TcpServer : public Application
{
public:
  TcpServer ();
  virtual ~TcpServer ();

  static TypeId GetTypeId (void);
  void StartApplication (void);
  void StopApplication (void);

private:
  void HandleAccept (Ptr<Socket> s, const Address& from);
  void HandleRead (Ptr<Socket> socket);

  Ptr<Socket> m_socket;
  std::vector<Ptr<Socket>> m_sockets;
};

TcpServer::TcpServer ()
  : m_socket (0)
{
}

TcpServer::~TcpServer ()
{
  m_socket = 0;
  m_sockets.clear ();
}

TypeId
TcpServer::GetTypeId (void)
{
  static TypeId tid = TypeId ("TcpServer")
    .SetParent<Application> ()
    .AddConstructor<TcpServer> ();
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
      for (auto iter = m_sockets.begin (); iter != m_sockets.end (); ++iter)
        {
          (*iter)->Close ();
        }
      m_sockets.clear ();
    }
}

void
TcpServer::HandleAccept (Ptr<Socket> s, const Address& from)
{
  NS_LOG_INFO ("Server accepted connection from " << from);
  s->SetRecvCallback (MakeCallback (&TcpServer::HandleRead, this));
  m_sockets.push_back (s);
}

void
TcpServer::HandleRead (Ptr<Socket> socket)
{
  Ptr<Packet> packet;
  Address from;
  while ((packet = socket->RecvFrom (from)))
    {
      if (packet->GetSize () > 0)
        {
          char *buffer = new char[packet->GetSize () + 1];
          packet->CopyData (buffer, packet->GetSize ());
          buffer[packet->GetSize ()] = '\0';
          NS_LOG_INFO ("Server received data: " << buffer << " from " << from);
          delete[] buffer;
        }
    }
}

class TcpClient : public Application
{
public:
  TcpClient ();
  virtual ~TcpClient ();

  static TypeId GetTypeId (void);
  void StartApplication (void);
  void StopApplication (void);

private:
  void SendData (Ptr<Socket> socket);
  void ConnectionSucceeded (Ptr<Socket>);
  void ConnectionFailed (Ptr<Socket>, Socket::SocketErrno);

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
    .AddConstructor<TcpClient> ();
  return tid;
}

void
TcpClient::StartApplication (void)
{
  TypeId tid = TypeId::LookupByName ("ns3::TcpSocketFactory");
  m_socket = Socket::CreateSocket (GetNode (), tid);
  m_socket->SetConnectCallback (
    MakeCallback (&TcpClient::ConnectionSucceeded, this),
    MakeCallback (&TcpClient::ConnectionFailed, this));
  m_socket->Connect (InetSocketAddress (m_serverIp, m_serverPort));
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
TcpClient::ConnectionSucceeded (Ptr<Socket> socket)
{
  NS_LOG_INFO ("Client connected to server.");
  Simulator::Schedule (Seconds (1.0), &TcpClient::SendData, this, socket);
}

void
TcpClient::ConnectionFailed (Ptr<Socket> socket, Socket::SocketErrno errno_)
{
  NS_LOG_ERROR ("Connection failed with error " << errno_);
}

void
TcpClient::SendData (Ptr<Socket> socket)
{
  std::string message = "Hello from TCP client!";
  Ptr<Packet> packet = Create<Packet> ((uint8_t*)message.c_str (), message.size ());
  socket->Send (packet);
  NS_LOG_INFO ("Client sent data: " << message);
}

int main (int argc, char *argv[])
{
  LogComponentEnable ("TcpSocketSimulation", LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create (2);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer devices = p2p.Install (nodes);

  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  TcpServerHelper server;
  ApplicationContainer serverApp = server.Install (nodes.Get (1));
  serverApp.Start (Seconds (1.0));
  serverApp.Stop (Seconds (10.0));

  TcpClientHelper client;
  ApplicationContainer clientApp = client.Install (nodes.Get (0));
  clientApp.Start (Seconds (2.0));
  clientApp.Stop (Seconds (10.0));

  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}