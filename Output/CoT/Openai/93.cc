#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("LanUdpTosTtlExample");

class UdpTosTtlSender : public Application
{
public:
  UdpTosTtlSender ();
  virtual ~UdpTosTtlSender ();
  void SetRemote (Address address, uint16_t port);
  void SetPacketSize (uint32_t size);
  void SetNumPackets (uint32_t num);
  void SetInterval (Time interval);
  void SetTos (uint8_t tos);
  void SetTtl (uint8_t ttl);
protected:
  virtual void StartApplication (void);
  virtual void StopApplication (void);
private:
  void SendPacket ();
  Ptr<Socket>     m_socket;
  Address         m_remoteAddress;
  uint16_t        m_remotePort;
  EventId         m_sendEvent;
  uint32_t        m_packetSize;
  uint32_t        m_nPackets;
  uint32_t        m_packetsSent;
  Time            m_interval;
  uint8_t         m_tos;
  uint8_t         m_ttl;
};

UdpTosTtlSender::UdpTosTtlSender ()
  : m_socket (0),
    m_remoteAddress (),
    m_remotePort (0),
    m_sendEvent (),
    m_packetSize (1024),
    m_nPackets (10),
    m_packetsSent (0),
    m_interval (Seconds(1.0)),
    m_tos (0),
    m_ttl (64)
{
}

UdpTosTtlSender::~UdpTosTtlSender ()
{
  m_socket = 0;
}

void
UdpTosTtlSender::SetRemote (Address address, uint16_t port)
{
  m_remoteAddress = address;
  m_remotePort = port;
}

void
UdpTosTtlSender::SetPacketSize (uint32_t size)
{
  m_packetSize = size;
}

void
UdpTosTtlSender::SetNumPackets (uint32_t num)
{
  m_nPackets = num;
}

void
UdpTosTtlSender::SetInterval (Time interval)
{
  m_interval = interval;
}

void
UdpTosTtlSender::SetTos (uint8_t tos)
{
  m_tos = tos;
}

void
UdpTosTtlSender::SetTtl (uint8_t ttl)
{
  m_ttl = ttl;
}

void
UdpTosTtlSender::StartApplication ()
{
  if (!m_socket)
    {
      m_socket = Socket::CreateSocket (GetNode (), UdpSocketFactory::GetTypeId ());
      m_socket->SetIpTos (m_tos);
      m_socket->SetIpTtl (m_ttl);
    }
  m_socket->Connect (InetSocketAddress (InetSocketAddress::ConvertFrom (m_remoteAddress).GetIpv4 (), m_remotePort));
  m_packetsSent = 0;
  SendPacket ();
}

void
UdpTosTtlSender::StopApplication ()
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
UdpTosTtlSender::SendPacket ()
{
  Ptr<Packet> packet = Create<Packet> (m_packetSize);
  m_socket->Send (packet);
  ++m_packetsSent;
  if (m_packetsSent < m_nPackets)
    {
      m_sendEvent = Simulator::Schedule (m_interval, &UdpTosTtlSender::SendPacket, this);
    }
}

class UdpTosTtlReceiver : public Application
{
public:
  UdpTosTtlReceiver ();
  virtual ~UdpTosTtlReceiver ();
  void SetPort (uint16_t port);
  void SetRecvTos (bool enable);
  void SetRecvTtl (bool enable);
protected:
  virtual void StartApplication (void);
  virtual void StopApplication (void);
private:
  void HandleRead (Ptr<Socket> socket);
  Ptr<Socket> m_socket;
  uint16_t    m_port;
  bool        m_recvTos;
  bool        m_recvTtl;
};

UdpTosTtlReceiver::UdpTosTtlReceiver ()
  : m_socket (0),
    m_port (9),
    m_recvTos (false),
    m_recvTtl (false)
{
}

UdpTosTtlReceiver::~UdpTosTtlReceiver ()
{
  m_socket = 0;
}

void
UdpTosTtlReceiver::SetPort (uint16_t port)
{
  m_port = port;
}

void
UdpTosTtlReceiver::SetRecvTos (bool enable)
{
  m_recvTos = enable;
}

void
UdpTosTtlReceiver::SetRecvTtl (bool enable)
{
  m_recvTtl = enable;
}

void
UdpTosTtlReceiver::StartApplication ()
{
  if (!m_socket)
    {
      m_socket = Socket::CreateSocket (GetNode (), UdpSocketFactory::GetTypeId ());
      InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), m_port);
      m_socket->Bind (local);

      if (m_recvTos)
        {
          m_socket->SetAttribute ("IpRecvTos", BooleanValue (true));
        }
      if (m_recvTtl)
        {
          m_socket->SetAttribute ("IpRecvTtl", BooleanValue (true));
        }

      m_socket->SetRecvPktInfo (true);
    }
  m_socket->SetRecvCallback (MakeCallback (&UdpTosTtlReceiver::HandleRead, this));
}

void
UdpTosTtlReceiver::StopApplication ()
{
  if (m_socket)
    {
      m_socket->SetRecvCallback (MakeNullCallback<void, Ptr<Socket> >());
      m_socket->Close ();
    }
}

void
UdpTosTtlReceiver::HandleRead (Ptr<Socket> socket)
{
  Address from;
  while (Ptr<Packet> packet = socket->RecvFrom (from))
    {
      SocketIpTosTag tosTag;
      SocketIpTtlTag ttlTag;
      uint8_t tos = 0, ttl = 0;

      if (packet->PeekPacketTag (tosTag))
        {
          tos = tosTag.GetTos ();
        }
      if (packet->PeekPacketTag (ttlTag))
        {
          ttl = ttlTag.GetTtl ();
        }

      NS_LOG_UNCOND ("At " << Simulator::Now ().GetSeconds () << "s receiver got "
        << packet->GetSize () << " bytes from " << InetSocketAddress::ConvertFrom (from).GetIpv4 ()
        << " TOS: " << unsigned (tos)
        << " TTL: " << unsigned (ttl));
    }
}

int main (int argc, char *argv[])
{
  uint32_t packetSize = 1024;
  uint32_t nPackets = 10;
  double interval = 1.0;
  uint8_t tos = 0;
  uint8_t ttl = 64;
  bool recvtos = false;
  bool recvttl = false;

  CommandLine cmd (__FILE__);
  cmd.AddValue ("packetSize", "Size of packets in bytes", packetSize);
  cmd.AddValue ("nPackets", "Number of packets to send", nPackets);
  cmd.AddValue ("interval", "Interval between packets (seconds)", interval);
  cmd.AddValue ("TOS", "Set IP_TOS value for sender packets (0-255)", tos);
  cmd.AddValue ("TTL", "Set IP_TTL value for sender packets", ttl);
  cmd.AddValue ("RECVTOS", "Enable receiver IP_RECVTOS option", recvtos);
  cmd.AddValue ("RECVTTL", "Enable receiver IP_RECVTTL option", recvttl);
  cmd.Parse (argc, argv);

  NodeContainer nodes;
  nodes.Create (2);

  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", StringValue ("100Mbps"));
  csma.SetChannelAttribute ("Delay", TimeValue (NanoSeconds (6560)));

  NetDeviceContainer devices = csma.Install (nodes);

  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");

  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  uint16_t port = 9;

  Ptr<UdpTosTtlReceiver> receiver = CreateObject<UdpTosTtlReceiver> ();
  receiver->SetPort (port);
  receiver->SetRecvTos (recvtos);
  receiver->SetRecvTtl (recvttl);
  nodes.Get (1)->AddApplication (receiver);
  receiver->SetStartTime (Seconds (0.0));
  receiver->SetStopTime (Seconds (interval * nPackets + 2.0));

  Ptr<UdpTosTtlSender> sender = CreateObject<UdpTosTtlSender> ();
  sender->SetRemote (Address (interfaces.GetAddress (1)), port);
  sender->SetPacketSize (packetSize);
  sender->SetNumPackets (nPackets);
  sender->SetInterval (Seconds (interval));
  sender->SetTos (tos);
  sender->SetTtl (ttl);
  nodes.Get (0)->AddApplication (sender);
  sender->SetStartTime (Seconds (1.0));
  sender->SetStopTime (Seconds (interval * nPackets + 2.0));

  Simulator::Stop (Seconds (interval * nPackets + 3.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}