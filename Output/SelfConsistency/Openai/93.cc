#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-header.h"
#include "ns3/ipv4-raw-socket-factory.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("UdpTosTtlCsmaExample");

class CustomReceiver : public Application
{
public:
  CustomReceiver ();
  virtual ~CustomReceiver ();
  
  void SetRecvTos (bool en) { m_enableRecvTos = en; }
  void SetRecvTtl (bool en) { m_enableRecvTtl = en; }

protected:
  virtual void StartApplication (void);
  virtual void StopApplication (void);

private:
  void HandleRead (Ptr<Socket> socket);
  Ptr<Socket>     m_socket;
  Address         m_local;
  bool            m_enableRecvTos;
  bool            m_enableRecvTtl;
};

CustomReceiver::CustomReceiver ()
  : m_socket (0),
    m_enableRecvTos (false),
    m_enableRecvTtl (false)
{
}

CustomReceiver::~CustomReceiver ()
{
  m_socket = 0;
}

void
CustomReceiver::StartApplication (void)
{
  if (!m_socket)
    {
      m_socket = Socket::CreateSocket (GetNode (), UdpSocketFactory::GetTypeId ());
      m_local = InetSocketAddress (Ipv4Address::GetAny (), 9000);
      m_socket->Bind (m_local);

      // Enable RECVTOS/RECVTTL if specified
      if (m_enableRecvTos)
        {
          int val = 1;
          m_socket->SetSockOpt (Socket::IP_RECVTOS, &val, sizeof(val));
        }
      if (m_enableRecvTtl)
        {
          int val = 1;
          m_socket->SetSockOpt (Socket::IP_RECVTTL, &val, sizeof(val));
        }
    }
  m_socket->SetRecvCallback (MakeCallback (&CustomReceiver::HandleRead, this));
}

void
CustomReceiver::StopApplication (void)
{
  if (m_socket)
    {
      m_socket->SetRecvCallback (MakeNullCallback<void, Ptr<Socket> > ());
      m_socket->Close ();
    }
}

void
CustomReceiver::HandleRead (Ptr<Socket> socket)
{
  Address from;
  while (Ptr<Packet> packet = socket->RecvFrom (from))
    {
      SocketIpTosTag tosTag;
      SocketIpTtlTag ttlTag;
      bool haveTos = packet->PeekPacketTag (tosTag);
      bool haveTtl = packet->PeekPacketTag (ttlTag);

      NS_LOG_UNCOND ("[Receiver] Received "
                     << packet->GetSize () << " bytes from "
                     << InetSocketAddress::ConvertFrom (from).GetIpv4 ()
                     << " port " << InetSocketAddress::ConvertFrom (from).GetPort ()
                     << (haveTos ? (" TOS=" + std::to_string(uint8_t(tosTag.Get ()))) : "")
                     << (haveTtl ? (" TTL=" + std::to_string(uint8_t(ttlTag.Get ()))) : ""));
    }
}

class CustomSender : public Application
{
public:
  CustomSender ();
  virtual ~CustomSender ();

  void Configure (Address remote, uint32_t pktSize, uint32_t nPkts, double interval,
                  int32_t tos, int32_t ttl);

protected:
  virtual void StartApplication (void);
  virtual void StopApplication (void);

private:
  void SendPacket ();
  EventId         m_sendEvent;
  Ptr<Socket>     m_socket;
  Address         m_remote;
  uint32_t        m_pktSize;
  uint32_t        m_sent;
  uint32_t        m_nPkts;
  Time            m_interval;
  int32_t         m_tos;
  int32_t         m_ttl;
};

CustomSender::CustomSender ()
  : m_sendEvent (),
    m_socket (0),
    m_pktSize (512),
    m_sent (0),
    m_nPkts (1),
    m_interval (Seconds(1.0)),
    m_tos (-1),
    m_ttl (-1)
{
}

CustomSender::~CustomSender ()
{
  m_socket = 0;
}

void
CustomSender::Configure (Address remote, uint32_t pktSize, uint32_t nPkts,
                        double interval, int32_t tos, int32_t ttl)
{
  m_remote = remote;
  m_pktSize = pktSize;
  m_nPkts = nPkts;
  m_interval = Seconds (interval);
  m_tos = tos;
  m_ttl = ttl;
}

void
CustomSender::StartApplication (void)
{
  if (!m_socket)
    {
      m_socket = Socket::CreateSocket (GetNode (), UdpSocketFactory::GetTypeId ());
    }
  // Set TOS (DSCP) if specified
  if (m_tos >= 0)
    {
      int tosVal = uint8_t (m_tos);
      m_socket->SetIpTos (tosVal);
    }
  // Set TTL if specified
  if (m_ttl >= 0)
    {
      int ttlVal = uint8_t (m_ttl);
      m_socket->SetIpTtl (ttlVal);
    }
  m_socket->Connect (m_remote);
  m_sent = 0;
  m_sendEvent = Simulator::Schedule (Seconds (0.0), &CustomSender::SendPacket, this);
}

void
CustomSender::StopApplication (void)
{
  if (m_sendEvent.IsRunning ())
    Simulator::Cancel (m_sendEvent);

  if (m_socket)
    m_socket->Close ();
}

void
CustomSender::SendPacket ()
{
  Ptr<Packet> packet = Create<Packet> (m_pktSize);
  m_socket->Send (packet);
  m_sent++;

  if (m_sent < m_nPkts)
    {
      m_sendEvent = Simulator::Schedule (m_interval, &CustomSender::SendPacket, this);
    }
}

int
main (int argc, char *argv[])
{
  uint32_t pktSize = 512;
  uint32_t nPackets = 10;
  double interval = 1.0;
  int32_t tos = -1;
  int32_t ttl = -1;
  bool recvTos = false;
  bool recvTtl = false;

  CommandLine cmd;
  cmd.AddValue ("packetSize", "Packet size in bytes", pktSize);
  cmd.AddValue ("numPackets", "Number of packets to send", nPackets);
  cmd.AddValue ("interval", "Interval between packets (seconds)", interval);
  cmd.AddValue ("tos", "IP_TOS field value for outgoing packets (-1 for default)", tos);
  cmd.AddValue ("ttl", "IP_TTL field value for outgoing packets (-1 for default)", ttl);
  cmd.AddValue ("recvTos", "Enable SO_RECVTOS on receiver", recvTos);
  cmd.AddValue ("recvTtl", "Enable SO_RECVTTL on receiver", recvTtl);
  cmd.Parse (argc, argv);

  // Create nodes and CSMA link
  NodeContainer nodes;
  nodes.Create (2);

  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", DataRateValue (DataRate ("100Mbps")));
  csma.SetChannelAttribute ("Delay", TimeValue (NanoSeconds (6560)));

  NetDeviceContainer devices = csma.Install (nodes);

  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer ifs = ipv4.Assign (devices);

  // Application setup
  uint16_t port = 9000;
  Address receiverAddress (InetSocketAddress (ifs.GetAddress (1), port));
  
  // Receiver on n1
  Ptr<CustomReceiver> receiver = CreateObject<CustomReceiver> ();
  receiver->SetRecvTos (recvTos);
  receiver->SetRecvTtl (recvTtl);
  nodes.Get (1)->AddApplication (receiver);
  receiver->SetStartTime (Seconds (0.0));
  receiver->SetStopTime (Seconds (20.0));
  
  // Sender on n0
  Ptr<CustomSender> sender = CreateObject<CustomSender> ();
  sender->Configure (receiverAddress, pktSize, nPackets, interval, tos, ttl);
  nodes.Get (0)->AddApplication (sender);
  sender->SetStartTime (Seconds (1.0));
  sender->SetStopTime (Seconds (20.0));

  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}