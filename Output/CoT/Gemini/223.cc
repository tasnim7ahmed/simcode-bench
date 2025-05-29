#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/socket.h"
#include "ns3/udp-socket-factory.h"
#include "ns3/packet.h"
#include "ns3/log.h"
#include "ns3/string.h"
#include "ns3/uinteger.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("AdhocWifiUdp");

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
  void SocketSend (void);

  Address m_peerAddress;
  uint32_t m_packetSize;
  uint32_t m_numPackets;
  DataRate m_dataRate;
  Socket *m_socket;
  EventId m_sendEvent;
  bool m_running;
  uint32_t m_packetsSent;
};

UdpClient::UdpClient ()
  : m_peerAddress (),
    m_packetSize (0),
    m_numPackets (0),
    m_dataRate (0),
    m_socket (0),
    m_sendEvent (),
    m_running (false),
    m_packetsSent (0)
{
}

UdpClient::~UdpClient ()
{
  if (m_socket != 0)
  {
    m_socket->Close ();
    m_socket->Dispose ();
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
  if (m_socket->Bind () == -1)
    {
      NS_FATAL_ERROR ("Failed to bind socket");
    }
  m_socket->Connect (m_peerAddress);
  m_socket->SetRecvCallback (MakeNullCallback<void, Ptr<Socket>, Ptr<Packet>, const Address &> ());

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

  if (m_socket != 0)
    {
      m_socket->Close ();
      m_socket->Dispose ();
    }
}

void
UdpClient::SendPacket (void)
{
  if (m_running)
    {
      Time interPacketInterval = Time (Seconds (m_packetSize * 8.0 / m_dataRate.GetBitRate ()));
      m_sendEvent = Simulator::Schedule (interPacketInterval, &UdpClient::SendPacket, this);
      SocketSend ();
    }
}

void
UdpClient::SocketSend (void)
{
  Ptr<Packet> packet = Create<Packet> (m_packetSize);
  m_socket->Send (packet);

  if (++m_packetsSent >= m_numPackets)
    {
      m_running = false;
    }
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
  void PrintReceivedPacket (Ptr<Socket> socket, Ptr<Packet> packet, const Address &from);

  uint16_t m_port;
  Socket *m_socket;
  bool m_running;
};

UdpServer::UdpServer () :
  m_port (0),
  m_socket (0),
  m_running (false)
{
}

UdpServer::~UdpServer()
{
    if (m_socket != 0)
    {
        m_socket->Close();
        m_socket->Dispose();
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
   if (m_socket != 0)
    {
        m_socket->Close();
        m_socket->Dispose();
    }
}

void
UdpServer::HandleRead (Ptr<Socket> socket)
{
  Address from;
  Ptr<Packet> packet;
  while ((packet = socket->RecvFrom (from)))
    {
      PrintReceivedPacket (socket, packet, from);
    }
}

void
UdpServer::PrintReceivedPacket (Ptr<Socket> socket, Ptr<Packet> packet, const Address &from)
{
  NS_LOG_UNCOND ("Received one packet!");
}

int
main (int argc, char *argv[])
{
  bool verbose = false;
  uint32_t packetSize = 1024;
  uint32_t numPackets = 100;
  DataRate dataRate ("1Mbps");
  double simulationTime = 10.0;

  CommandLine cmd;
  cmd.AddValue ("verbose", "Tell echo applications to log if true", verbose);
  cmd.AddValue ("packetSize", "Size of echo packet", packetSize);
  cmd.AddValue ("numPackets", "Number of packets sent", numPackets);
  cmd.AddValue ("dataRate", "Sending rate", dataRate);
  cmd.AddValue ("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.Parse (argc, argv);

  if (verbose)
    {
      LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
      LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);
    }

  NodeContainer nodes;
  nodes.Create (2);

  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211b);

  YansWifiPhyHelper wifiPhy =  YansWifiPhyHelper::Default ();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
  wifiPhy.SetChannel (wifiChannel.Create ());

  WifiMacHelper wifiMac;
  wifiMac.SetType ("ns3::AdhocWifiMac");
  NetDeviceContainer devices = wifi.Install (wifiPhy, wifiMac, nodes);

  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (5.0),
                                 "DeltaY", DoubleValue (10.0),
                                 "GridWidth", UintegerValue (3),
                                 "LayoutType", StringValue ("RowFirst"));
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (nodes);

  InternetStackHelper internet;
  internet.Install (nodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i = ipv4.Assign (devices);

  uint16_t port = 9;

  UdpServerHelper echoServer (port);

  Ptr<Node> appServer = nodes.Get (1);
  Ptr<UdpServer> app = CreateObject<UdpServer> ();
  app->Setup (port);
  appServer->AddApplication (app);
  app->SetStartTime (Seconds (1.0));
  app->SetStopTime (Seconds (simulationTime));

  Address serverAddress (InetSocketAddress (i.GetAddress (1), port));

  Ptr<Node> appClient = nodes.Get (0);
  Ptr<UdpClient> client = CreateObject<UdpClient> ();
  client->Setup (serverAddress, packetSize, numPackets, dataRate);
  appClient->AddApplication (client);
  client->SetStartTime (Seconds (2.0));
  client->SetStopTime (Seconds (simulationTime));

  Simulator::Stop (Seconds (simulationTime));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}