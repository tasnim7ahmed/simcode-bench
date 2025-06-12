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
    if (m_socket->Bind (local) == -1)
    {
      NS_FATAL_ERROR ("Failed to bind socket");
    }
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
      NS_LOG_INFO ("Server received " << packet->GetSize () << " bytes");
    }
  }));
}

class TcpClient : public Application
{
public:
  TcpClient ();
  virtual ~TcpClient ();

  static TypeId GetTypeId (void);
  void SendData ();

private:
  virtual void StartApplication (void);
  virtual void StopApplication (void);

  Ptr<Socket> m_socket;
  EventId m_sendEvent;
  uint32_t m_packetsSent;
};

TcpClient::TcpClient ()
  : m_socket (0), m_packetsSent (0)
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
  TypeId tid = TypeId::LookupByName ("ns3::TcpSocketFactory");
  m_socket = Socket::CreateSocket (GetNode (), tid);

  m_socket->Connect (InetSocketAddress (Ipv4Address ("10.1.1.2"), 8080));
  m_socket->SetConnectCallback (
    MakeCallback ([](Ptr<Socket> socket){}),
    MakeCallback ([](Ptr<Socket> socket){}));

  m_sendEvent = Simulator::Schedule (Seconds (1.0), &TcpClient::SendData, this);
}

void
TcpClient::StopApplication (void)
{
  if (m_sendEvent.IsRunning ())
  {
    Simulator::Cancel (m_sendEvent);
  }

  if (m_socket)
  {
    m_socket->Close ();
  }
}

void
TcpClient::SendData ()
{
  if (m_packetsSent < 5)
  {
    Ptr<Packet> packet = Create<Packet> (1024); // 1024-byte packet
    m_socket->Send (packet);
    NS_LOG_INFO ("Client sent packet " << m_packetsSent << ", size: 1024 bytes");
    m_packetsSent++;
    m_sendEvent = Simulator::Schedule (Seconds (1.0), &TcpClient::SendData, this);
  }
}

int
main (int argc, char *argv[])
{
  LogComponentEnable ("TcpSocketExample", LOG_LEVEL_INFO);

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

  ApplicationContainer serverApp;
  TcpServerHelper server;
  serverApp.Add (server.Install (nodes.Get (1)));
  serverApp.Start (Seconds (0.5));
  serverApp.Stop (Seconds (10.0));

  ApplicationContainer clientApp;
  TcpClientHelper client;
  clientApp.Add (client.Install (nodes.Get (0)));
  clientApp.Start (Seconds (0.5));
  clientApp.Stop (Seconds (10.0));

  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}