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
  void ReceivedResponse (Ptr<Socket> socket);
  void CloseSocket (void);

  Address m_peerAddress;
  uint32_t m_packetSize;
  uint32_t m_numPackets;
  DataRate m_dataRate;
  EventId m_sendEvent;
  Ptr<Socket> m_socket;
  uint32_t m_packetsSent;
  bool m_running;
};

UdpClient::UdpClient ()
  : m_peerAddress (),
    m_packetSize (0),
    m_numPackets (0),
    m_dataRate (0),
    m_sendEvent (),
    m_socket (nullptr),
    m_packetsSent (0),
    m_running (false)
{
}

UdpClient::~UdpClient()
{
  if (m_socket)
    {
      m_socket->Close ();
    }
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
  m_running = true;
  m_packetsSent = 0;

  m_socket = Socket::CreateSocket (GetNode (), UdpSocketFactory::GetTypeId ());
  if (m_socket == nullptr)
    {
      NS_FATAL_ERROR ("Failed to create socket");
    }
  m_socket->Bind ();
  m_socket->Connect (m_peerAddress);

  SendPacket ();
}

void
UdpClient::StopApplication (void)
{
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
  Ptr<Packet> packet = Create<Packet> (m_packetSize);
  m_socket->Send (packet);

  if (++m_packetsSent < m_numPackets)
    {
      Time tNext (Seconds (m_packetSize * 8.0 / m_dataRate.GetBitRate ()));
      m_sendEvent = Simulator::Schedule (tNext, &UdpClient::SendPacket, this);
    }
}

void
UdpClient::ReceivedResponse (Ptr<Socket> socket)
{
  Address from;
  Ptr<Packet> packet = socket->RecvFrom (from);
  if (packet)
    {
      NS_LOG_INFO ("Received " << packet->GetSize () << " bytes from " << from);
    }
}

void
UdpClient::CloseSocket (void)
{
  m_socket->Close ();
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
  void AcceptConnection (Ptr<Socket> socket, const Address& from);
  void ConnectionRequest (Ptr<Socket> socket);

  uint16_t m_port;
  Ptr<Socket> m_socket;
  bool m_running;
};

UdpServer::UdpServer () : m_port (0), m_socket (nullptr), m_running (false)
{
}

UdpServer::~UdpServer()
{
  if (m_socket)
    {
      m_socket->Close ();
    }
}

void
UdpServer::Setup (uint16_t port)
{
  m_port = port;
}

void
UdpServer::StartApplication (void)
{
  m_running = true;
  m_socket = Socket::CreateSocket (GetNode (), UdpSocketFactory::GetTypeId ());
  if (m_socket == nullptr)
    {
      NS_FATAL_ERROR ("Failed to create socket");
    }
  InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), m_port);
  if (m_socket->Bind (local) == -1)
    {
      NS_FATAL_ERROR ("Failed to bind socket");
    }

  m_socket->SetRecvCallback (MakeCallback (&UdpServer::HandleRead, this));
}

void
UdpServer::StopApplication (void)
{
  m_running = false;

  if (m_socket)
    {
      m_socket->Close ();
    }
}

void
UdpServer::HandleRead (Ptr<Socket> socket)
{
  Address from;
  Ptr<Packet> packet;
  while ((packet = socket->RecvFrom (from)))
    {
      uint8_t *buffer = new uint8_t[packet->GetSize ()];
      packet->CopyData (buffer, packet->GetSize ());
      std::cout << GetNode ()->GetId () << " received " << packet->GetSize () << " bytes from " << InetSocketAddress::ConvertFrom (from).GetIpv4 () << " port " << InetSocketAddress::ConvertFrom (from).GetPort () << std::endl;
      delete[] buffer;
    }
}

int main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  NodeContainer nodes;
  nodes.Create (2);

  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211b);

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Create ();
  wifiPhy.SetChannel (wifiChannel.Create ());

  WifiMacHelper wifiMac;
  wifiMac.SetType ("ns3::AdhocWifiMac");

  NetDeviceContainer devices = wifi.Install (wifiPhy, wifiMac, nodes);

  InternetStackHelper internet;
  internet.Install (nodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = ipv4.Assign (devices);

  uint16_t port = 9;

  UdpServerHelper server (port);
  ApplicationContainer serverApps = server.Install (nodes.Get (1));
  Ptr<UdpServer> serverApp = DynamicCast<UdpServer>(serverApps.Get(0));
  serverApp->Setup(port);
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  UdpClientHelper client (interfaces.GetAddress (1), port);
  client.SetAttribute ("MaxPackets", UintegerValue (1000000));
  client.SetAttribute ("PacketSize", UintegerValue (1024));
  ApplicationContainer clientApps = client.Install (nodes.Get (0));

  Ptr<UdpClient> clientApp = DynamicCast<UdpClient>(clientApps.Get(0));
  clientApp->Setup (InetSocketAddress (interfaces.GetAddress (1), port), 1024, 1000, DataRate ("1Mbps"));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}