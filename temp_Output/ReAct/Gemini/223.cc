#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/socket.h"
#include "ns3/udp-socket-factory.h"
#include "ns3/packet.h"
#include "ns3/uinteger.h"
#include "ns3/string.h"
#include <iostream>

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
  void ScheduleTx (void);

  Address m_peerAddress;
  uint32_t m_packetSize;
  uint32_t m_numPackets;
  DataRate m_dataRate;
  uint32_t m_packetsSent;
  bool m_running;
  Time m_startTime;
  Time m_stopTime;
  Time m_interPacketInterval;
  Socket m_socket;
  EventId m_sendEvent;
};

UdpClient::UdpClient ()
  : m_peerAddress (),
    m_packetSize (0),
    m_numPackets (0),
    m_dataRate (0),
    m_packetsSent (0),
    m_running (false),
    m_startTime (),
    m_stopTime (),
    m_interPacketInterval (),
    m_socket (nullptr),
    m_sendEvent ()
{
}

UdpClient::~UdpClient()
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
  m_interPacketInterval = Time (Seconds (m_packetSize * 8.0 / m_dataRate.GetBitRate ()));
}

void
UdpClient::StartApplication (void)
{
  m_running = true;
  m_packetsSent = 0;

  m_socket = Socket::CreateSocket (GetNode (), UdpSocketFactory::GetTypeId ());
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
      ScheduleTx ();
    }
}

void
UdpClient::ScheduleTx (void)
{
  if (m_running)
    {
      m_sendEvent = Simulator::Schedule (m_interPacketInterval, &UdpClient::SendPacket, this);
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
  void PacketReceived (Ptr<Socket> socket);

  uint16_t m_port;
  Socket m_socket;
  bool m_running;
  Time m_startTime;
  Time m_stopTime;
  Address m_localAddress;
};

UdpServer::UdpServer ()
  : m_port (0),
    m_socket (nullptr),
    m_running (false),
    m_startTime (),
    m_stopTime (),
    m_localAddress ()
{
}

UdpServer::~UdpServer()
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
  m_running = true;

  m_socket = Socket::CreateSocket (GetNode (), UdpSocketFactory::GetTypeId ());
  InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), m_port);
  m_socket->Bind (local);
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
      if (packet->GetSize () > 0)
        {
          NS_LOG_UNCOND ("Received one packet!");
        }
    }
}

int main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);
  LogComponentEnable ("AdhocUdp", LOG_LEVEL_INFO);

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

  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  uint16_t port = 9;
  UdpServer serverApp;
  serverApp.Setup (port);

  Ptr<Node> appServerNode = nodes.Get (1);
  serverApp.SetStartTime (Seconds (0.0));
  serverApp.SetStopTime (Seconds (10.0));
  appServerNode->AddApplication (&serverApp);

  UdpClient clientApp;
  Address serverAddress (InetSocketAddress (interfaces.GetAddress (1), port));
  clientApp.Setup (serverAddress, 1024, 1000, DataRate ("1Mbps"));

  Ptr<Node> appClientNode = nodes.Get (0);
  clientApp.SetStartTime (Seconds (1.0));
  clientApp.SetStopTime (Seconds (10.0));
  appClientNode->AddApplication (&clientApp);

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}