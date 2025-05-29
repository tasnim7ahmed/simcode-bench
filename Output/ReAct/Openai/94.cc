#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("Ipv6UdpTclassHoplimit");

class UdpIpv6Server : public Application
{
public:
  UdpIpv6Server ();
  virtual ~UdpIpv6Server ();

  void Setup (uint16_t port, bool recvTclass, bool recvHoplimit);

private:
  virtual void StartApplication (void);
  virtual void StopApplication (void);

  void HandleRead (Ptr<Socket> socket);

  Ptr<Socket> m_socket;
  uint16_t m_port;
  bool m_recvTclass;
  bool m_recvHoplimit;
};

UdpIpv6Server::UdpIpv6Server ()
  : m_socket (0),
    m_port (0),
    m_recvTclass (false),
    m_recvHoplimit (false)
{
}

UdpIpv6Server::~UdpIpv6Server ()
{
  m_socket = 0;
}

void
UdpIpv6Server::Setup (uint16_t port, bool recvTclass, bool recvHoplimit)
{
  m_port = port;
  m_recvTclass = recvTclass;
  m_recvHoplimit = recvHoplimit;
}

void
UdpIpv6Server::StartApplication (void)
{
  m_socket = Socket::CreateSocket (GetNode (), UdpSocketFactory::GetTypeId ());
  m_socket->SetRecvPktInfo (true);
#if defined(SOL_IPV6) && defined(IPV6_RECVTCLASS)
  if (m_recvTclass)
    {
      int optval = 1;
      m_socket->SetSockOpt (SOL_IPV6, IPV6_RECVTCLASS, &optval, sizeof(optval));
    }
#endif
#if defined(SOL_IPV6) && defined(IPV6_RECVHOPLIMIT)
  if (m_recvHoplimit)
    {
      int optval = 1;
      m_socket->SetSockOpt (SOL_IPV6, IPV6_RECVHOPLIMIT, &optval, sizeof(optval));
    }
#endif
  Inet6SocketAddress local = Inet6SocketAddress (Ipv6Address::GetAny (), m_port);
  m_socket->Bind (local);
  m_socket->SetRecvCallback (MakeCallback (&UdpIpv6Server::HandleRead, this));
}

void
UdpIpv6Server::StopApplication ()
{
  if (m_socket)
    {
      m_socket->SetRecvCallback (MakeNullCallback<void, Ptr<Socket> > ());
      m_socket->Close ();
    }
}

void
UdpIpv6Server::HandleRead (Ptr<Socket> socket)
{
  Address from;
  Ptr<Packet> packet;
  while ((packet = socket->RecvFrom (from)))
    {
      // Get TCLASS and HOPLIMIT ancillary data if present
      int tclass = -1;
      int hoplimit = -1;
      SocketIpTosTag tosTag;
      SocketHopLimitTag hopLimitTag;
      if (packet->PeekPacketTag (tosTag))
        {
          tclass = tosTag.GetTos ();
        }
      if (packet->PeekPacketTag (hopLimitTag))
        {
          hoplimit = hopLimitTag.GetHopLimit ();
        }
      Inet6SocketAddress address = Inet6SocketAddress::ConvertFrom (from);
      NS_LOG_UNCOND ("Received " << packet->GetSize ()
          << " bytes from " << address.GetIpv6 ()
          << " port " << address.GetPort ()
          << " TCLASS=" << tclass
          << " HOPLIMIT=" << hoplimit
          << " at " << Simulator::Now ().GetSeconds () << "s");
    }
}

class UdpIpv6Client : public Application
{
public:
  UdpIpv6Client ();
  virtual ~UdpIpv6Client ();

  void Setup (Ipv6Address address, uint16_t port, uint32_t packetSize,
              uint32_t nPackets, Time interval, int tclass, int hoplimit);

private:
  virtual void StartApplication (void);
  virtual void StopApplication (void);
  void ScheduleTx (void);
  void SendPacket (void);

  Ptr<Socket> m_socket;
  Ipv6Address m_peerAddress;
  uint16_t m_peerPort;
  uint32_t m_packetSize;
  uint32_t m_nPackets;
  Time m_interval;
  uint32_t m_sent;
  EventId m_sendEvent;
  int m_tclass;
  int m_hoplimit;
};

UdpIpv6Client::UdpIpv6Client ()
  : m_socket (0),
    m_peerAddress (),
    m_peerPort (0),
    m_packetSize (0),
    m_nPackets (0),
    m_interval (Seconds (1.0)),
    m_sent (0),
    m_tclass (-1),
    m_hoplimit (-1)
{
}

UdpIpv6Client::~UdpIpv6Client ()
{
  m_socket = 0;
}

void
UdpIpv6Client::Setup (Ipv6Address address, uint16_t port, uint32_t packetSize,
                      uint32_t nPackets, Time interval, int tclass, int hoplimit)
{
  m_peerAddress = address;
  m_peerPort = port;
  m_packetSize = packetSize;
  m_nPackets = nPackets;
  m_interval = interval;
  m_tclass = tclass;
  m_hoplimit = hoplimit;
}

void
UdpIpv6Client::StartApplication ()
{
  m_sent = 0;
  m_socket = Socket::CreateSocket (GetNode (), UdpSocketFactory::GetTypeId ());

#if defined (SOL_IPV6) && defined (IPV6_TCLASS)
  if (m_tclass >= 0)
    {
      int val = m_tclass;
      m_socket->SetSockOpt (SOL_IPV6, IPV6_TCLASS, &val, sizeof(val));
    }
#endif
#if defined(SOL_IPV6) && defined(IPV6_HOPLIMIT)
  if (m_hoplimit > 0)
    {
      int val = m_hoplimit;
      m_socket->SetSockOpt (SOL_IPV6, IPV6_HOPLIMIT, &val, sizeof(val));
    }
#endif

  m_socket->Connect (Inet6SocketAddress (m_peerAddress, m_peerPort));
  SendPacket ();
}

void
UdpIpv6Client::StopApplication ()
{
  if (m_socket)
    {
      m_socket->Close ();
    }
  Simulator::Cancel (m_sendEvent);
}

void
UdpIpv6Client::SendPacket ()
{
  Ptr<Packet> packet = Create<Packet> (m_packetSize);

  // Explicitly tag the packet to help demonstrate TCLASS/HOPLIMIT in ns-3 when available
  if (m_tclass >= 0)
    {
      SocketIpTosTag tosTag;
      tosTag.SetTos (m_tclass);
      packet->AddPacketTag (tosTag);
    }
  if (m_hoplimit > 0)
    {
      SocketHopLimitTag hlTag;
      hlTag.SetHopLimit (m_hoplimit);
      packet->AddPacketTag (hlTag);
    }

  m_socket->Send (packet);
  ++m_sent;
  if (m_sent < m_nPackets)
    {
      ScheduleTx ();
    }
}

void
UdpIpv6Client::ScheduleTx ()
{
  m_sendEvent = Simulator::Schedule (m_interval, &UdpIpv6Client::SendPacket, this);
}

int
main (int argc, char *argv[])
{
  uint32_t packetSize = 512;
  uint32_t nPackets = 10;
  double interval = 1.0;
  int ipv6Tclass = -1;
  int ipv6Hoplimit = -1;
  bool recvTclass = true;
  bool recvHoplimit = true;
  uint16_t port = 9999;

  CommandLine cmd;
  cmd.AddValue ("packetSize", "Size of application packet sent", packetSize);
  cmd.AddValue ("nPackets", "Number of packets to send", nPackets);
  cmd.AddValue ("interval", "Interval between packets (seconds)", interval);
  cmd.AddValue ("ipv6Tclass", "IPv6 Traffic Class value (TCLASS) to set on socket", ipv6Tclass);
  cmd.AddValue ("ipv6Hoplimit", "IPv6 Hop Limit value to set on socket", ipv6Hoplimit);
  cmd.AddValue ("recvTclass", "Receiver enables IPV6_RECVTCLASS option", recvTclass);
  cmd.AddValue ("recvHoplimit", "Receiver enables IPV6_RECVHOPLIMIT option", recvHoplimit);
  cmd.AddValue ("port", "UDP port to use", port);
  cmd.Parse (argc, argv);

  NodeContainer nodes;
  nodes.Create (2);

  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", StringValue ("100Mbps"));
  csma.SetChannelAttribute ("Delay", TimeValue (NanoSeconds (6560)));
  NetDeviceContainer devices = csma.Install (nodes);

  InternetStackHelper internetv6;
  internetv6.Install (nodes);

  Ipv6AddressHelper ipv6;
  ipv6.SetBase (Ipv6Address ("2001:1::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer ifs = ipv6.Assign (devices);
  ifs.SetForwarding (0, true);
  ifs.SetForwarding (1, true);
  ifs.SetDefaultRouteInAllNodes (0);

  // Server on n1
  Ptr<UdpIpv6Server> serverApp = CreateObject<UdpIpv6Server> ();
  serverApp->Setup (port, recvTclass, recvHoplimit);
  nodes.Get (1)->AddApplication (serverApp);
  serverApp->SetStartTime (Seconds (0.0));
  serverApp->SetStopTime (Seconds (interval * nPackets + 5));

  // Client on n0
  Ptr<UdpIpv6Client> clientApp = CreateObject<UdpIpv6Client> ();
  clientApp->Setup (ifs.GetAddress (1, 1), port, packetSize, nPackets, Seconds (interval), ipv6Tclass, ipv6Hoplimit);
  nodes.Get (0)->AddApplication (clientApp);
  clientApp->SetStartTime (Seconds (1.0));
  clientApp->SetStopTime (Seconds (interval * nPackets + 2));

  Simulator::Stop (Seconds (interval * nPackets + 10));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}