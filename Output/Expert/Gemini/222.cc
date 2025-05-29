#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/log.h"
#include "ns3/socket.h"
#include "ns3/udp-socket-factory.h"
#include "ns3/packet.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiUdpCustom");

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
  void SocketRecv (Ptr<Socket> socket);

  Address m_peerAddress;
  uint32_t m_packetSize;
  uint32_t m_numPackets;
  DataRate m_dataRate;
  Ptr<Socket> m_socket;
  Address m_localAddress;
  uint32_t m_packetsSent;
  EventId m_sendEvent;
  bool m_running;
};

UdpClient::UdpClient () :
  m_packetSize (1024),
  m_numPackets (100),
  m_dataRate (0),
  m_socket (0),
  m_localAddress (),
  m_packetsSent (0),
  m_sendEvent (),
  m_running (false)
{
}

UdpClient::~UdpClient()
{
  if (m_socket)
  {
    m_socket->Close ();
    m_socket->SetRecvCallback (MakeNullCallback<void, Ptr<Socket> > ());
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
  m_socket->Bind ();
  m_socket->SetRecvCallback (MakeCallback (&UdpClient::SocketRecv, this));
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
UdpClient::SocketRecv (Ptr<Socket> socket)
{
  Address from;
  Ptr<Packet> packet = socket->RecvFrom (from);
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
  void ConnectionClosed (Ptr<Socket> socket);

  uint16_t m_port;
  Ptr<Socket> m_socket;
  bool m_connected;
};

UdpServer::UdpServer () :
  m_port (0),
  m_socket (0),
  m_connected (false)
{
}

UdpServer::~UdpServer()
{
  if (m_socket)
  {
    m_socket->Close ();
    m_socket->SetRecvCallback (MakeNullCallback<void, Ptr<Socket> > ());
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
  m_socket = Socket::CreateSocket (GetNode (), UdpSocketFactory::GetTypeId ());
  InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), m_port);
  m_socket->Bind (local);
  m_socket->SetRecvCallback (MakeCallback (&UdpServer::HandleRead, this));
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
UdpServer::HandleRead (Ptr<Socket> socket)
{
  Address from;
  Ptr<Packet> packet;
  while ((packet = socket->RecvFrom (from)))
  {
    std::cout << Simulator::Now ().AsDouble () << "s " << GetNode ()->GetId () << " Received one packet!" << std::endl;
  }
}

void
UdpServer::AcceptConnection (Ptr<Socket> socket, const Address& from)
{
  NS_LOG_FUNCTION (this << socket << from);
}

void
UdpServer::ConnectionClosed (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << socket);
}


int main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);

  NodeContainer nodes;
  nodes.Create (3);

  NodeContainer clientNode = NodeContainer (nodes.Get (0));
  NodeContainer apNode = NodeContainer (nodes.Get (1));
  NodeContainer serverNode = NodeContainer (nodes.Get (2));

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy;
  phy.SetChannel (channel.Create ());

  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211b);

  WifiMacHelper mac;
  Ssid ssid = Ssid ("ns3-wifi");

  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid),
               "ActiveProbing", BooleanValue (false));

  NetDeviceContainer clientDevices = wifi.Install (phy, mac, clientNode);

  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid));

  NetDeviceContainer apDevices = wifi.Install (phy, mac, apNode);

  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid),
               "ActiveProbing", BooleanValue (false));
  NetDeviceContainer serverDevices = wifi.Install (phy, mac, serverNode);

  InternetStackHelper internet;
  internet.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer clientInterfaces = address.Assign (clientDevices);
  Ipv4InterfaceContainer apInterfaces = address.Assign (apDevices);
  Ipv4InterfaceContainer serverInterfaces = address.Assign (serverDevices);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  uint16_t port = 9;
  UdpServerHelper server (port);

  Ptr<UdpServer> serverApp = CreateObject<UdpServer> ();
  serverApp->Setup (port);
  serverNode.Get (0)->AddApplication (serverApp);
  serverApp->SetStartTime (Seconds (1.0));
  serverApp->SetStopTime (Seconds (10.0));

  Address serverAddress (InetSocketAddress (serverInterfaces.GetAddress (0), port));

  Ptr<UdpClient> clientApp = CreateObject<UdpClient> ();
  clientApp->Setup (serverAddress, 1024, 1000, DataRate ("1Mbps"));
  clientNode.Get (0)->AddApplication (clientApp);
  clientApp->SetStartTime (Seconds (2.0));
  clientApp->SetStopTime (Seconds (10.0));

  Simulator::Stop (Seconds (10.0));

  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}