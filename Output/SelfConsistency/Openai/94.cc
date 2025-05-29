// lan-udp-ipv6-tclass-hoplimit.cc
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv6-address-helper.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("LanUdpIpv6TclassHoplimitExample");

class UdpIpv6Receiver : public Application
{
public:
  UdpIpv6Receiver();
  virtual ~UdpIpv6Receiver();

  void Setup(Address address, uint16_t port,
             bool recvTclass, bool recvHopLimit);

private:
  virtual void StartApplication(void) override;
  virtual void StopApplication(void) override;

  void HandleRead(Ptr<Socket> socket);

  Ptr<Socket>     m_socket;
  Address         m_local;
  uint16_t        m_port;
  bool            m_running;
  bool            m_recvTclass;
  bool            m_recvHopLimit;
};

UdpIpv6Receiver::UdpIpv6Receiver()
  : m_socket(0),
    m_port(0),
    m_running(false),
    m_recvTclass(false),
    m_recvHopLimit(false)
{
}

UdpIpv6Receiver::~UdpIpv6Receiver()
{
  m_socket = 0;
}

void UdpIpv6Receiver::Setup(Address address,
                            uint16_t port,
                            bool recvTclass,
                            bool recvHopLimit)
{
  m_local = address;
  m_port = port;
  m_recvTclass = recvTclass;
  m_recvHopLimit = recvHopLimit;
}

void UdpIpv6Receiver::StartApplication(void)
{
  if (m_socket == 0)
  {
    m_socket = Socket::CreateSocket(GetNode(), TypeId::LookupByName("ns3::UdpSocketFactory"));
    m_socket->Bind(Inet6SocketAddress(Ipv6Address::ConvertFrom(m_local), m_port));

    int optval = 1;
#if defined(SOL_IPV6) && defined(IPV6_RECVTCLASS)
    if (m_recvTclass)
    {
      NS_ABORT_MSG_IF(m_socket->SetSockOpt(SOL_IPV6, IPV6_RECVTCLASS, &optval, sizeof(optval)) < 0,
                      "Failed to set IPV6_RECVTCLASS");
    }
#endif
#if defined(SOL_IPV6) && defined(IPV6_RECVHOPLIMIT)
    if (m_recvHopLimit)
    {
      NS_ABORT_MSG_IF(m_socket->SetSockOpt(SOL_IPV6, IPV6_RECVHOPLIMIT, &optval, sizeof(optval)) < 0,
                      "Failed to set IPV6_RECVHOPLIMIT");
    }
#endif
  }
  m_socket->SetRecvCallback(MakeCallback(&UdpIpv6Receiver::HandleRead, this));
  m_running = true;
}

void UdpIpv6Receiver::StopApplication(void)
{
  m_running = false;
  if (m_socket)
  {
    m_socket->SetRecvCallback(MakeNullCallback<void, Ptr<Socket>>());
    m_socket->Close();
  }
}

void UdpIpv6Receiver::HandleRead(Ptr<Socket> socket)
{
  Address from;
  Ptr<Packet> packet;
  struct SocketIpv6HopLimitTag
  {
    int hoplimit;
  };
  struct SocketIpv6TrafficClassTag
  {
    int tclass;
  };

  while ((packet = socket->RecvFrom(from)))
  {
    int tclass = -1;
    int hoplimit = -1;

#if defined(SOL_IPV6) && defined(IPV6_TCLASS)
    if (m_recvTclass)
    {
      socklen_t optlen = sizeof(tclass);
      if (socket->GetSockOpt(SOL_IPV6, IPV6_TCLASS, &tclass, &optlen) < 0)
      {
        tclass = -1;
      }
    }
#endif
#if defined(SOL_IPV6) && defined(IPV6_HOPLIMIT)
    if (m_recvHopLimit)
    {
      socklen_t optlen = sizeof(hoplimit);
      if (socket->GetSockOpt(SOL_IPV6, IPV6_HOPLIMIT, &hoplimit, &optlen) < 0)
      {
        hoplimit = -1;
      }
    }
#endif
    NS_LOG_UNCOND("Received "
        << packet->GetSize()
        << " bytes from "
        << Inet6SocketAddress::ConvertFrom(from).GetIpv6()
        << " TCLASS=" << tclass
        << " HOPLIMIT=" << hoplimit
        << " at " << Simulator::Now().GetSeconds() << "s");
  }
}

class UdpIpv6Sender : public Application
{
public:
  UdpIpv6Sender();
  virtual ~UdpIpv6Sender();

  void Setup(Address address, uint16_t port,
             uint32_t packetSize, uint32_t nPackets, Time interval,
             int tclass, int hoplimit);

private:
  virtual void StartApplication(void) override;
  virtual void StopApplication(void) override;

  void ScheduleTx(void);
  void SendPacket(void);

  Ptr<Socket>     m_socket;
  Address         m_peer;
  uint16_t        m_port;
  EventId         m_sendEvent;
  uint32_t        m_packetSize;
  uint32_t        m_nPackets;
  Time            m_interval;
  uint32_t        m_packetsSent;
  int             m_tclass;
  int             m_hoplimit;
};

UdpIpv6Sender::UdpIpv6Sender()
  : m_socket(0),
    m_port(0),
    m_packetSize(0),
    m_nPackets(0),
    m_interval(Seconds(1.0)),
    m_packetsSent(0),
    m_tclass(-1),
    m_hoplimit(-1)
{
}

UdpIpv6Sender::~UdpIpv6Sender()
{
  m_socket = 0;
}

void UdpIpv6Sender::Setup(Address address,
                          uint16_t port,
                          uint32_t packetSize,
                          uint32_t nPackets,
                          Time interval,
                          int tclass,
                          int hoplimit)
{
  m_peer = address;
  m_port = port;
  m_packetSize = packetSize;
  m_nPackets = nPackets;
  m_interval = interval;
  m_tclass = tclass;
  m_hoplimit = hoplimit;
}

void UdpIpv6Sender::StartApplication(void)
{
  m_packetsSent = 0;
  if (m_socket == 0)
  {
    m_socket = Socket::CreateSocket(GetNode(), TypeId::LookupByName("ns3::UdpSocketFactory"));
    m_socket->Connect(Inet6SocketAddress(Ipv6Address::ConvertFrom(m_peer), m_port));
#if defined(SOL_IPV6) && defined(IPV6_TCLASS)
    if (m_tclass >= 0)
    {
      NS_ABORT_MSG_IF(m_socket->SetSockOpt(SOL_IPV6, IPV6_TCLASS, &m_tclass, sizeof(m_tclass)) < 0,
                      "Failed to set IPV6_TCLASS");
    }
#endif
#if defined(SOL_IPV6) && defined(IPV6_HOPLIMIT)
    if (m_hoplimit >= 0)
    {
      NS_ABORT_MSG_IF(m_socket->SetSockOpt(SOL_IPV6, IPV6_HOPLIMIT, &m_hoplimit, sizeof(m_hoplimit)) < 0,
                      "Failed to set IPV6_HOPLIMIT");
    }
#endif
  }
  SendPacket();
}

void UdpIpv6Sender::StopApplication(void)
{
  if (m_socket)
  {
    m_socket->Close();
  }
  Simulator::Cancel(m_sendEvent);
}

void UdpIpv6Sender::SendPacket(void)
{
  Ptr<Packet> packet = Create<Packet>(m_packetSize);
  m_socket->Send(packet);

  ++m_packetsSent;
  if (m_packetsSent < m_nPackets)
  {
    ScheduleTx();
  }
}

void UdpIpv6Sender::ScheduleTx(void)
{
  m_sendEvent = Simulator::Schedule(m_interval, &UdpIpv6Sender::SendPacket, this);
}

int main(int argc, char *argv[])
{
  // Default parameters
  uint32_t packetSize = 1024;
  uint32_t nPackets = 3;
  double interval = 1.0;
  int ipv6Tclass = -1;
  int ipv6HopLimit = -1;
  bool recvTclass = true;
  bool recvHopLimit = true;

  CommandLine cmd(__FILE__);
  cmd.AddValue("packetSize", "Size of application packet sent", packetSize);
  cmd.AddValue("nPackets", "Number of packets to send", nPackets);
  cmd.AddValue("interval", "Interval between packets (s)", interval);
  cmd.AddValue("ipv6Tclass", "IPv6 traffic class to set (default: -1 / unset)", ipv6Tclass);
  cmd.AddValue("ipv6HopLimit", "IPv6 hop limit to set (default: -1 / unset)", ipv6HopLimit);
  cmd.AddValue("recvTclass", "Receiver: enable RECVTCLASS option", recvTclass);
  cmd.AddValue("recvHopLimit", "Receiver: enable RECVHOPLIMIT option", recvHopLimit);
  cmd.Parse(argc, argv);

  Time interPacketInterval = Seconds(interval);

  // Create two nodes
  NodeContainer nodes;
  nodes.Create(2);

  // Set up CSMA channel
  CsmaHelper csma;
  csma.SetChannelAttribute("DataRate", DataRateValue(DataRate("100Mbps")));
  csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(1)));

  NetDeviceContainer devices = csma.Install(nodes);

  // Install IPv6 Internet stack
  InternetStackHelper internetv6;
  internetv6.Install(nodes);

  // Assign IPv6 addresses
  Ipv6AddressHelper ipv6;
  ipv6.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
  Ipv6InterfaceContainer ifaces = ipv6.Assign(devices);
  ifaces.SetForwarding(0, true);
  ifaces.SetDefaultRouteInAllNodes(0);

  // Application parameters
  uint16_t port = 5000;

  // Receiver (n1) application
  Ptr<UdpIpv6Receiver> receiver = CreateObject<UdpIpv6Receiver>();
  receiver->Setup(ifaces.GetAddress(1, 1), port, recvTclass, recvHopLimit);
  nodes.Get(1)->AddApplication(receiver);
  receiver->SetStartTime(Seconds(0.0));
  receiver->SetStopTime(Seconds(nPackets * interval + 2.0));

  // Sender (n0) application
  Ptr<UdpIpv6Sender> sender = CreateObject<UdpIpv6Sender>();
  sender->Setup(ifaces.GetAddress(1, 1), port, packetSize, nPackets, interPacketInterval, ipv6Tclass, ipv6HopLimit);
  nodes.Get(0)->AddApplication(sender);
  sender->SetStartTime(Seconds(1.0));
  sender->SetStopTime(Seconds(nPackets * interval + 1.0));

  Simulator::Stop(Seconds(nPackets * interval + 3.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}