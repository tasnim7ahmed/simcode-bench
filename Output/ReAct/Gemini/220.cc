#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/socket.h"
#include "ns3/udp-socket-factory.h"
#include "ns3/packet.h"
#include "ns3/log.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("UdpRawSocketExample");

class UdpClient : public Application
{
public:
  UdpClient ();
  virtual ~UdpClient ();

  void Setup (Address address, uint32_t packetSize, uint32_t numPackets, DataRate dataRate);

private:
  virtual void StartApplication (void);
  virtual void StopApplication (void);

  void SendPacket (void);
  void ScheduleTx (void);

  Address m_peerAddress;
  uint32_t m_packetSize;
  uint32_t m_numPackets;
  DataRate m_dataRate;
  Socket m_socket;
  EventId m_sendEvent;
  uint32_t m_packetsSent;
};

UdpClient::UdpClient ()
  : m_peerAddress (),
    m_packetSize (0),
    m_numPackets (0),
    m_dataRate (0),
    m_socket (nullptr),
    m_sendEvent (),
    m_packetsSent (0)
{
}

UdpClient::~UdpClient ()
{
  m_socket = nullptr;
}

void
UdpClient::Setup (Address address, uint32_t packetSize, uint32_t numPackets, DataRate dataRate)
{
  m_peerAddress = address;
  m_packetSize = packetSize;
  m_numPackets = numPackets;
  m_dataRate = dataRate;
}

void
UdpClient::StartApplication (void)
{
  m_socket = Socket::CreateSocket (GetNode (), UdpSocketFactory::GetTypeId ());
  if (m_socket->Bind () == -1)
    {
      NS_FATAL_ERROR ("Failed to bind socket");
    }
  m_socket->Connect (m_peerAddress);
  m_socket->SetRecvCallback (MakeNullCallback<void,Ptr<Socket>,Ptr<Packet>,const Address&> ());
  m_packetsSent = 0;
  ScheduleTx ();
}

void
UdpClient::StopApplication (void)
{
  Simulator::Cancel (m_sendEvent);
  if (m_socket)
    {
      m_socket->Close ();
    }
}

void
UdpClient::SendPacket (void)
{
  Ptr<Packet> packet = Create<Packet> (m_packetSize);
  m_socket->Send (packet);

  if (++m_packetsSent < m_numPackets)
    {
      ScheduleTx ();
    }
}

void
UdpClient::ScheduleTx (void)
{
  Time tNext (Seconds (m_packetSize * 8.0 / m_dataRate.GetBitRate ()));
  m_sendEvent = Simulator::Schedule (tNext, &UdpClient::SendPacket, this);
}

class UdpServer : public Application
{
public:
  UdpServer ();
  virtual ~UdpServer ();

  void Setup (uint16_t port);

private:
  virtual void StartApplication (void);
  virtual void StopApplication (void);

  void ReceivePacket (Ptr<Socket> socket);

  uint16_t m_port;
  Socket m_socket;
  Address m_localAddress;
};

UdpServer::UdpServer ()
  : m_port (0),
    m_socket (nullptr),
    m_localAddress ()
{
}

UdpServer::~UdpServer ()
{
  m_socket = nullptr;
}

void
UdpServer::Setup (uint16_t port)
{
  m_port = port;
}

void
UdpServer::StartApplication (void)
{
  m_socket = Socket::CreateSocket (GetNode (), UdpSocketFactory::GetTypeId ());
  InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), m_port);
  if (m_socket->Bind (local) == -1)
    {
      NS_FATAL_ERROR ("Failed to bind socket");
    }

  m_socket->SetRecvCallback (MakeCallback (&UdpServer::ReceivePacket, this));
}

void
UdpServer::StopApplication (void)
{
  if (m_socket)
    {
      m_socket->Close ();
    }
}

void
UdpServer::ReceivePacket (Ptr<Socket> socket)
{
  Address from;
  Ptr<Packet> packet = socket->RecvFrom (from);
  NS_LOG_UNCOND ("Received one packet!");
}

int
main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);
  LogComponentEnable ("UdpRawSocketExample", LOG_LEVEL_INFO);

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

  uint16_t port = 9;
  UdpServerHelper server (port);
  ApplicationContainer serverApps = server.Install (nodes.Get (1));
  Ptr<UdpServer> serverApp = StaticCast<UdpServer>(serverApps.Get(0));
  serverApp->Setup(port);
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  uint32_t packetSize = 1024;
  uint32_t numPackets = 1000;
  DataRate dataRate ("1Mbps");
  UdpClientHelper client (interfaces.GetAddress (1), port);
  ApplicationContainer clientApps = client.Install (nodes.Get (0));
    Ptr<UdpClient> clientApp = StaticCast<UdpClient>(clientApps.Get(0));
    clientApp->Setup(interfaces.GetAddress (1), packetSize, numPackets, dataRate);
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}