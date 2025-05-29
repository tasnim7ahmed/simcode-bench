#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/socket.h"
#include "ns3/udp-socket-factory.h"
#include "ns3/packet.h"
#include "ns3/uinteger.h"
#include "ns3/string.h"
#include "ns3/log.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("AdhocUdp");

class UdpClient : public Application
{
public:
  UdpClient ();
  virtual ~UdpClient();

  void Setup (Address address, uint32_t packetSize, uint32_t numPackets, DataRate dataRate);

private:
  virtual void StartApplication (void);
  virtual void StopApplication (void);

  void SendPacket (void);
  void SocketReceive (Ptr<Socket> socket);

  Address m_peerAddress;
  uint32_t m_packetSize;
  uint32_t m_numPackets;
  DataRate m_dataRate;
  Ptr<Socket> m_socket;
  Address m_localAddress;
  uint32_t m_packetsSent;
  bool m_running;
  Time m_sendInterval;
  EventId m_sendEvent;
};

UdpClient::UdpClient ()
  : m_packetSize (1024)
  , m_numPackets (1)
  , m_packetsSent (0)
  , m_running (false)
{
  NS_LOG_FUNCTION (this);
}

UdpClient::~UdpClient()
{
  NS_LOG_FUNCTION (this);
}

void
UdpClient::Setup (Address address, uint32_t packetSize, uint32_t numPackets, DataRate dataRate)
{
  NS_LOG_FUNCTION (this << address << packetSize << numPackets << dataRate);
  m_peerAddress = address;
  m_packetSize = packetSize;
  m_numPackets = numPackets;
  m_dataRate = dataRate;
  m_sendInterval = Time (Seconds (8 * (double) packetSize / dataRate.GetBitRate ()));
}

void
UdpClient::StartApplication (void)
{
  NS_LOG_FUNCTION (this);
  m_running = true;
  m_packetsSent = 0;

  m_socket = Socket::CreateSocket (GetNode (), UdpSocketFactory::GetTypeId ());
  if (m_socket->Bind () == -1)
    {
      NS_FATAL_ERROR ("Failed to bind socket");
    }

  m_socket->Connect (m_peerAddress);
  m_socket->SetRecvCallback (MakeCallback (&UdpClient::SocketReceive, this));

  SendPacket ();
}

void
UdpClient::StopApplication (void)
{
  NS_LOG_FUNCTION (this);
  m_running = false;

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
UdpClient::SendPacket (void)
{
  NS_LOG_FUNCTION (this);
  Ptr<Packet> packet = Create<Packet> (m_packetSize);
  m_socket->Send (packet);

  if (++m_packetsSent < m_numPackets)
    {
      m_sendEvent = Simulator::Schedule (m_sendInterval, &UdpClient::SendPacket, this);
    }
}

void
UdpClient::SocketReceive (Ptr<Socket> socket)
{
  Address from;
  Ptr<Packet> packet = socket->RecvFrom (from);
  NS_LOG_INFO ("Received one packet!");
}

class UdpServer : public Application
{
public:
  UdpServer ();
  virtual ~UdpServer();

  void Setup (uint16_t port);

private:
  virtual void StartApplication (void);
  virtual void StopApplication (void);

  void HandleRead (Ptr<Socket> socket);
  void ReportRx ();

  uint16_t m_port;
  Ptr<Socket> m_socket;
  bool m_running;
  EventId m_report;
  uint32_t m_bytesReceived;
};

UdpServer::UdpServer () : m_port (0), m_running (false), m_bytesReceived(0)
{
  NS_LOG_FUNCTION (this);
}

UdpServer::~UdpServer()
{
  NS_LOG_FUNCTION (this);
}

void
UdpServer::Setup (uint16_t port)
{
  NS_LOG_FUNCTION (this << port);
  m_port = port;
}

void
UdpServer::StartApplication (void)
{
  NS_LOG_FUNCTION (this);
  m_running = true;

  m_socket = Socket::CreateSocket (GetNode (), UdpSocketFactory::GetTypeId ());
  InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), m_port);
  if (m_socket->Bind (local) == -1)
    {
      NS_FATAL_ERROR ("Failed to bind socket");
    }

  m_socket->SetRecvCallback (MakeCallback (&UdpServer::HandleRead, this));

  m_report = Simulator::Schedule (Seconds (1.0), &UdpServer::ReportRx, this);

}

void
UdpServer::StopApplication (void)
{
  NS_LOG_FUNCTION (this);
  m_running = false;

  if (m_socket)
    {
      m_socket->Close ();
    }
    Simulator::Cancel (m_report);
}

void
UdpServer::HandleRead (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << socket);
  Address from;
  Ptr<Packet> packet;
  while ((packet = socket->RecvFrom (from)))
    {
      m_bytesReceived += packet->GetSize ();
      NS_LOG_INFO ("Received " << packet->GetSize () << " bytes from " << from);
    }
}

void
UdpServer::ReportRx ()
{
  NS_LOG_FUNCTION (this);
  NS_LOG_INFO ("Server Bytes Received = " << m_bytesReceived);
  m_report = Simulator::Schedule (Seconds (1.0), &UdpServer::ReportRx, this);
}

int
main (int argc, char *argv[])
{
  LogComponentEnable ("AdhocUdp", LOG_LEVEL_INFO);
  CommandLine cmd;
  cmd.Parse (argc, argv);

  NodeContainer nodes;
  nodes.Create (2);

  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211b);

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Create ();
  wifiPhy.SetChannel (wifiChannel.Create ());

  NqosWifiMacHelper wifiMac = NqosWifiMacHelper::Default ();
  wifiMac.SetType ("ns3::AdhocWifiMac");

  NetDeviceContainer devices = wifi.Install (wifiPhy, wifiMac, nodes);

  InternetStackHelper internet;
  internet.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  uint16_t port = 9;
  UdpServerHelper server (port);

  Ptr<Node> serverNode = nodes.Get (1);
  Ptr<UdpServer> app = DynamicCast<UdpServer> (server.Install (serverNode).Get (0));
  app->Setup (port);

  Ptr<Node> clientNode = nodes.Get (0);
  Ptr<UdpClient> clientApp = CreateObject<UdpClient> ();
  clientApp->Setup (InetSocketAddress (interfaces.GetAddress (1), port), 1024, 1000, DataRate ("100Kbps"));
  clientNode->AddApplication (clientApp);
  clientApp->SetStartTime (Seconds (1.0));
  clientApp->SetStopTime (Seconds (10.0));
  app->SetStartTime (Seconds (0.0));
  app->SetStopTime (Seconds (10.0));

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}