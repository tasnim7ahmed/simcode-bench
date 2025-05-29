#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/packet.h"
#include "ns3/uinteger.h"
#include "ns3/double.h"
#include "ns3/string.h"
#include "ns3/socket.h"
#include "ns3/udp-socket-factory.h"
#include "ns3/log.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiSenderReceiver");

class WifiSender : public Application
{
public:
  WifiSender ();
  virtual ~WifiSender ();

  void Setup (Address address, uint16_t port, uint32_t packetSize, uint32_t numPackets, Time interPacketInterval);

private:
  virtual void StartApplication (void);
  virtual void StopApplication (void);

  void SendPacket (void);
  void TraceAck (Ptr<const Packet> packet);

  Address m_peerAddress;
  uint16_t m_peerPort;
  uint32_t m_packetSize;
  uint32_t m_numPackets;
  Time m_interPacketInterval;
  Socket *m_socket = nullptr;
  uint32_t m_packetsSent;
  EventId m_sendEvent;
};

WifiSender::WifiSender () :
  m_peerAddress (),
  m_peerPort (0),
  m_packetSize (0),
  m_numPackets (0),
  m_interPacketInterval (Seconds (0)),
  m_socket (nullptr),
  m_packetsSent (0),
  m_sendEvent ()
{
}

WifiSender::~WifiSender ()
{
  if (m_socket != nullptr)
    {
      m_socket->Close ();
      Simulator::Destroy ();
    }
}

void
WifiSender::Setup (Address address, uint16_t port, uint32_t packetSize, uint32_t numPackets, Time interPacketInterval)
{
  m_peerAddress = address;
  m_peerPort = port;
  m_packetSize = packetSize;
  m_numPackets = numPackets;
  m_interPacketInterval = interPacketInterval;
}

void
WifiSender::StartApplication (void)
{
  m_socket = Socket::CreateSocket (GetNode (), UdpSocketFactory::GetTypeId ());
  if (m_socket == nullptr)
    {
      NS_FATAL_ERROR ("Failed to create socket");
    }
  m_socket->Bind ();
  m_socket->Connect (m_peerAddress);
  m_packetsSent = 0;
  SendPacket ();
}

void
WifiSender::StopApplication (void)
{
  Simulator::Cancel (m_sendEvent);
  if (m_socket != nullptr)
    {
      m_socket->Close ();
    }
}

void
WifiSender::SendPacket (void)
{
  Ptr<Packet> packet = Create<Packet> (m_packetSize);
  m_socket->Send (packet);

  if (++m_packetsSent < m_numPackets)
    {
      m_sendEvent = Simulator::Schedule (m_interPacketInterval, &WifiSender::SendPacket, this);
    }
}

void
WifiSender::TraceAck (Ptr<const Packet> packet)
{
  NS_LOG_FUNCTION (this << packet);
}

class WifiReceiver : public Application
{
public:
  WifiReceiver ();
  virtual ~WifiReceiver ();

  void Setup (uint16_t port);

private:
  virtual void StartApplication (void);
  virtual void StopApplication (void);

  void HandleRead (Ptr<Socket> socket);
  void PacketReceived (Ptr<Socket> socket);

  uint16_t m_port;
  Socket *m_socket = nullptr;
  Address m_localAddress;
};

WifiReceiver::WifiReceiver () :
  m_port (0),
  m_socket (nullptr),
  m_localAddress ()
{
}

WifiReceiver::~WifiReceiver ()
{
  if (m_socket != nullptr)
    {
      m_socket->Close ();
      Simulator::Destroy ();
    }
}

void
WifiReceiver::Setup (uint16_t port)
{
  m_port = port;
}

void
WifiReceiver::StartApplication (void)
{
  m_socket = Socket::CreateSocket (GetNode (), UdpSocketFactory::GetTypeId ());
  if (m_socket == nullptr)
    {
      NS_FATAL_ERROR ("Failed to create socket");
    }
  InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), m_port);
  m_socket->Bind (local);
  m_socket->SetRecvCallback (MakeCallback (&WifiReceiver::HandleRead, this));
}

void
WifiReceiver::StopApplication (void)
{
  if (m_socket != nullptr)
    {
      m_socket->Close ();
    }
}

void
WifiReceiver::HandleRead (Ptr<Socket> socket)
{
  Address from;
  Ptr<Packet> packet = socket->RecvFrom (from);
  Time arrival_time = Simulator::Now();
  Time sent_time;
  packet->RemovePacketTag(TimestampTag(sent_time));
  Time delay = arrival_time - sent_time;

  NS_LOG_UNCOND ("Received one packet " << packet->GetSize () << " bytes from " << InetSocketAddress::ConvertFrom(from).GetIpv4() << " port " << InetSocketAddress::ConvertFrom(from).GetPort() << " delay " << delay);

  PacketReceived (socket);
}

void
WifiReceiver::PacketReceived (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << socket);
}

int
main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);
  LogComponentEnable ("WifiSenderReceiver", LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create (2);

  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211b);

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Create ();
  wifiPhy.SetChannel (wifiChannel.Create ());

  WifiMacHelper wifiMac;
  Ssid ssid = Ssid ("ns-3-ssid");
  wifiMac.SetType ("ns3::StaWifiMac",
                   "Ssid", SsidValue (ssid),
                   "ActiveProbing", BooleanValue (false));

  NetDeviceContainer staDevices;
  staDevices = wifi.Install (wifiPhy, wifiMac, nodes.Get (1));

  wifiMac.SetType ("ns3::ApWifiMac",
                   "Ssid", SsidValue (ssid));

  NetDeviceContainer apDevices;
  apDevices = wifi.Install (wifiPhy, wifiMac, nodes.Get (0));

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
  Ipv4InterfaceContainer i = ipv4.Assign (apDevices);
  ipv4.Assign (staDevices);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  uint16_t port = 9;
  Address sinkLocalAddress (InetSocketAddress (Ipv4Address::GetAny (), port));
  Ptr<WifiReceiver> app_sink = CreateObject<WifiReceiver> ();
  app_sink->Setup (port);
  nodes.Get (1)->AddApplication (app_sink);
  app_sink->SetStartTime (Seconds (1.0));
  app_sink->SetStopTime (Seconds (10.0));

  Ptr<WifiSender> app_source = CreateObject<WifiSender> ();
  app_source->Setup (sinkLocalAddress, port, 1024, 1000, Seconds (0.001));
  nodes.Get (0)->AddApplication (app_source);
  app_source->SetStartTime (Seconds (2.0));
  app_source->SetStopTime (Seconds (10.0));

  Simulator::Stop (Seconds (11.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}