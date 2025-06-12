#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("LanTosTtlExample");

class UdpCustomSender : public Application
{
public:
  UdpCustomSender();
  virtual ~UdpCustomSender();

  void Setup(
    Address address,
    uint32_t packetSize,
    uint32_t nPackets,
    Time pktInterval,
    int32_t ipTos,
    int32_t ipTtl
  );

protected:
  virtual void StartApplication() override;
  virtual void StopApplication() override;

private:
  void ScheduleTx(void);
  void SendPacket(void);

  Ptr<Socket>     m_socket;
  Address         m_peerAddress;
  uint32_t        m_packetSize;
  uint32_t        m_nPackets;
  Time            m_pktInterval;
  uint32_t        m_packetsSent;
  EventId         m_sendEvent;
  int32_t         m_ipTos;
  int32_t         m_ipTtl;
};

UdpCustomSender::UdpCustomSender()
  : m_socket(0),
    m_packetSize(0),
    m_nPackets(0),
    m_pktInterval(Seconds(1.0)),
    m_packetsSent(0),
    m_ipTos(-1),
    m_ipTtl(-1)
{
}

UdpCustomSender::~UdpCustomSender()
{
  m_socket = 0;
}

void
UdpCustomSender::Setup(Address address, uint32_t packetSize, uint32_t nPackets,
                       Time pktInterval, int32_t ipTos, int32_t ipTtl)
{
  m_peerAddress = address;
  m_packetSize = packetSize;
  m_nPackets = nPackets;
  m_pktInterval = pktInterval;
  m_ipTos = ipTos;
  m_ipTtl = ipTtl;
}

void
UdpCustomSender::StartApplication()
{
  if (!m_socket)
  {
    m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
    // Set outbound TOS (if configured)
    if (m_ipTos >= 0)
    {
      m_socket->SetIpTos(m_ipTos);
    }
    // Set outbound TTL (if configured)
    if (m_ipTtl >= 0)
    {
      m_socket->SetIpTtl(m_ipTtl);
    }
    m_socket->Connect(m_peerAddress);
  }
  m_packetsSent = 0;
  SendPacket();
}

void
UdpCustomSender::StopApplication()
{
  if (m_socket)
  {
    m_socket->Close();
  }
  Simulator::Cancel(m_sendEvent);
}

void
UdpCustomSender::SendPacket()
{
  Ptr<Packet> packet = Create<Packet>(m_packetSize);
  m_socket->Send(packet);

  ++m_packetsSent;
  if (m_packetsSent < m_nPackets)
  {
    ScheduleTx();
  }
}

void
UdpCustomSender::ScheduleTx(void)
{
  m_sendEvent = Simulator::Schedule(m_pktInterval, &UdpCustomSender::SendPacket, this);
}

// Receiver Application
class UdpCustomReceiver : public Application
{
public:
  UdpCustomReceiver();
  virtual ~UdpCustomReceiver();

  void Setup(
    uint16_t port,
    bool recvTos,
    bool recvTtl
  );

protected:
  virtual void StartApplication() override;
  virtual void StopApplication() override;

private:
  void HandleRead(Ptr<Socket> socket);

  Ptr<Socket> m_socket;
  uint16_t    m_port;
  bool        m_recvTos;
  bool        m_recvTtl;
};

UdpCustomReceiver::UdpCustomReceiver()
  : m_socket(0),
    m_port(0),
    m_recvTos(false),
    m_recvTtl(false)
{
}

UdpCustomReceiver::~UdpCustomReceiver()
{
  m_socket = 0;
}

void
UdpCustomReceiver::Setup(uint16_t port, bool recvTos, bool recvTtl)
{
  m_port = port;
  m_recvTos = recvTos;
  m_recvTtl = recvTtl;
}

void
UdpCustomReceiver::StartApplication()
{
  if (!m_socket)
  {
    m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
    InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), m_port);
    m_socket->Bind(local);

    if (m_recvTos)
    {
      m_socket->SetRecvTos(true);
    }
    if (m_recvTtl)
    {
      m_socket->SetRecvTtl(true);
    }
    m_socket->SetRecvCallback(MakeCallback(&UdpCustomReceiver::HandleRead, this));
  }
}

void
UdpCustomReceiver::StopApplication()
{
  if (m_socket)
  {
    m_socket->Close();
  }
}

void
UdpCustomReceiver::HandleRead(Ptr<Socket> socket)
{
  Address from;
  while (Ptr<Packet> packet = m_socket->RecvFrom(from))
  {
    SocketIpTosTag tosTag;
    SocketIpTtlTag ttlTag;

    bool foundTos = false, foundTtl = false;

    uint8_t tos = 0;
    uint8_t ttl = 0;

    if (m_recvTos && packet->PeekPacketTag(tosTag))
    {
      tos = tosTag.GetTos();
      foundTos = true;
    }
    if (m_recvTtl && packet->PeekPacketTag(ttlTag))
    {
      ttl = ttlTag.GetTtl();
      foundTtl = true;
    }

    InetSocketAddress address = InetSocketAddress::ConvertFrom(from);
    NS_LOG_UNCOND("At time " << Simulator::Now().GetSeconds()
      << "s: received " << packet->GetSize() << " bytes from "
      << address.GetIpv4()
      << (foundTos ? (std::string(", TOS=") + std::to_string(tos)) : "")
      << (foundTtl ? (std::string(", TTL=") + std::to_string(ttl)) : "")
    );
  }
}

int main(int argc, char *argv[])
{
  uint32_t packetSize = 1024;
  uint32_t nPackets = 10;
  double interval = 1.0;
  int32_t ipTos = -1;
  int32_t ipTtl = -1;
  bool recvTos = false;
  bool recvTtl = false;

  // Command-line parameters
  CommandLine cmd;
  cmd.AddValue("packetSize", "Size of each UDP packet in bytes", packetSize);
  cmd.AddValue("nPackets", "Number of packets to send", nPackets);
  cmd.AddValue("interval", "Interval between packets (s)", interval);
  cmd.AddValue("TOS", "IP_TOS value of outgoing packets (0-255)", ipTos);
  cmd.AddValue("TTL", "IP_TTL value of outgoing packets (1-255)", ipTtl);
  cmd.AddValue("RECVTOS", "Set SO_RECVTOS on receiver (log received IP_TOS)", recvTos);
  cmd.AddValue("RECVTTL", "Set SO_RECVTTL on receiver (log received IP_TTL)", recvTtl);
  cmd.Parse(argc, argv);

  NodeContainer nodes;
  nodes.Create(2);

  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", StringValue ("100Mbps"));
  csma.SetChannelAttribute ("Delay", TimeValue (NanoSeconds(6560)));

  NetDeviceContainer devices = csma.Install(nodes);

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  uint16_t port = 4000;

  // Install receiver (n1)
  Ptr<UdpCustomReceiver> receiverApp = CreateObject<UdpCustomReceiver>();
  receiverApp->Setup(port, recvTos, recvTtl);
  nodes.Get(1)->AddApplication(receiverApp);
  receiverApp->SetStartTime(Seconds(0.0));
  receiverApp->SetStopTime(Seconds(100.0));

  // Install sender (n0)
  Ptr<UdpCustomSender> senderApp = CreateObject<UdpCustomSender>();
  Address peerAddr = InetSocketAddress(interfaces.GetAddress(1), port);
  senderApp->Setup(peerAddr, packetSize, nPackets, Seconds(interval), ipTos, ipTtl);
  nodes.Get(0)->AddApplication(senderApp);
  senderApp->SetStartTime(Seconds(1.0));
  senderApp->SetStopTime(Seconds(100.0));

  Simulator::Stop(Seconds(2.0 + nPackets * interval));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}