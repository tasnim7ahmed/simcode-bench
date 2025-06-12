#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-header.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("UdpTosTtlExample");

class UdpServer : public Application
{
public:
  static TypeId GetTypeId(void);
  UdpServer();
  virtual ~UdpServer();

protected:
  virtual void StartApplication(void);
  virtual void StopApplication(void);

private:
  void HandleRead(Ptr<Socket> socket);

  uint16_t m_port;
  Ptr<Socket> m_socket;
};

TypeId
UdpServer::GetTypeId(void)
{
  static TypeId tid = TypeId("UdpServer")
    .SetParent<Application>()
    .AddConstructor<UdpServer>()
    ;
  return tid;
}

UdpServer::UdpServer()
  : m_port(9)
{
}

UdpServer::~UdpServer()
{
}

void
UdpServer::StartApplication(void)
{
  if (!m_socket)
    {
      TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
      m_socket = Socket::CreateSocket(GetNode(), tid);
      InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), m_port);
      m_socket->Bind(local);
      m_socket->SetRecvCallback(MakeCallback(&UdpServer::HandleRead, this));
    }
}

void
UdpServer::StopApplication(void)
{
  if (m_socket)
    {
      m_socket->Close();
      m_socket = nullptr;
    }
}

void
UdpServer::HandleRead(Ptr<Socket> socket)
{
  Ptr<Packet> packet;
  Address from;
  while ((packet = socket->RecvFrom(from)))
    {
      Ipv4Header ipHeader;
      socket->GetSockName(from);
      packet->RemoveHeader(ipHeader);

      uint8_t tos = ipHeader.GetTos();
      uint8_t ttl = ipHeader.GetTtl();
      NS_LOG_UNCOND("Received packet: size=" << packet->GetSize() << " TOS=" << (uint32_t)tos << " TTL=" << (uint32_t)ttl);
    }
}

class UdpClient : public Application
{
public:
  static TypeId GetTypeId(void);
  UdpClient();
  virtual ~UdpClient();

  void Setup(Ptr<Socket> socket, Address address, uint32_t packetSize, uint32_t nPackets, Time interval, uint8_t ipTos, uint8_t ipTtl, bool enableTos, bool enableTtl);

protected:
  virtual void StartApplication(void);
  virtual void StopApplication(void);

private:
  void SendPacket(void);

  TypeId m_tid;
  uint32_t m_packetSize;
  uint32_t m_nPackets;
  DataRate m_dataRate;
  uint32_t m_packetsSent;
  Ptr<Socket> m_socket;
  Address m_peer;
  EventId m_sendEvent;
  uint8_t m_ipTos;
  uint8_t m_ipTtl;
  bool m_enableTos;
  bool m_enableTtl;
};

TypeId
UdpClient::GetTypeId(void)
{
  static TypeId tid = TypeId("UdpClient")
    .SetParent<Application>()
    .AddConstructor<UdpClient>()
    ;
  return tid;
}

UdpClient::UdpClient()
  : m_packetSize(512),
    m_nPackets(1),
    m_dataRate("1Mbps"),
    m_packetsSent(0),
    m_socket(nullptr),
    m_peer(),
    m_sendEvent(),
    m_ipTos(0),
    m_ipTtl(64),
    m_enableTos(false),
    m_enableTtl(false)
{
}

UdpClient::~UdpClient()
{
  m_sendEvent.Cancel();
}

void
UdpClient::Setup(Ptr<Socket> socket, Address address, uint32_t packetSize, uint32_t nPackets, Time interval, uint8_t ipTos, uint8_t ipTtl, bool enableTos, bool enableTtl)
{
  m_socket = socket;
  m_peer = address;
  m_packetSize = packetSize;
  m_nPackets = nPackets;
  m_dataRate = DataRate(interval.ToDataRate(m_packetSize));
  m_ipTos = ipTos;
  m_ipTtl = ipTtl;
  m_enableTos = enableTos;
  m_enableTtl = enableTtl;

  if (m_enableTos)
    {
      m_socket->SetIpTos(m_ipTos);
    }

  if (m_enableTtl)
    {
      m_socket->SetIpTtl(m_ipTtl);
    }
}

void
UdpClient::StartApplication(void)
{
  m_tid = TypeId::LookupByName("ns3::UdpSocketFactory");
  if (!m_socket)
    {
      m_socket = Socket::CreateSocket(GetNode(), m_tid);
    }

  m_socket->Connect(m_peer);
  SendPacket();
}

void
UdpClient::StopApplication(void)
{
  m_sendEvent.Cancel();
  if (m_socket)
    {
      m_socket->Close();
      m_socket = nullptr;
    }
}

void
UdpClient::SendPacket()
{
  Ptr<Packet> packet = Create<Packet>(m_packetSize);
  m_socket->Send(packet);

  if (++m_packetsSent < m_nPackets)
    {
      Time interval = Seconds((double)m_packetSize * 8 / m_dataRate.GetBitRate());
      m_sendEvent = Simulator::Schedule(interval, &UdpClient::SendPacket, this);
    }
}

int main(int argc, char *argv[])
{
  uint32_t packetSize = 512;
  uint32_t numPackets = 1;
  double interval = 1.0; // seconds
  uint8_t ipTos = 0;
  uint8_t ipTtl = 64;
  bool enableTos = false;
  bool enableTtl = false;
  bool enableRecvTos = true;
  bool enableRecvTtl = true;

  CommandLine cmd(__FILE__);
  cmd.AddValue("packetSize", "size of application packet sent", packetSize);
  cmd.AddValue("numPackets", "number of packets to send", numPackets);
  cmd.AddValue("interval", "interval (seconds) between packets", interval);
  cmd.AddValue("ipTos", "IP TOS value for outgoing packets", ipTos);
  cmd.AddValue("ipTtl", "IP TTL value for outgoing packets", ipTtl);
  cmd.AddValue("enableTos", "Enable setting IP_TOS on socket", enableTos);
  cmd.AddValue("enableTtl", "Enable setting IP_TTL on socket", enableTtl);
  cmd.AddValue("enableRecvTos", "Enable receiving TOS values at receiver", enableRecvTos);
  cmd.AddValue("enableRecvTtl", "Enable receiving TTL values at receiver", enableRecvTtl);
  cmd.Parse(argc, argv);

  Time interPacketInterval = Seconds(interval);

  NodeContainer nodes;
  nodes.Create(2);

  CsmaHelper csma;
  csma.SetChannelAttribute("DataRate", DataRateValue(DataRate(100e6)));
  csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));

  NetDeviceContainer devices = csma.Install(nodes);

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");

  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");

  // Server (n1)
  UdpServerHelper serverHelper;
  serverHelper.SetAttribute("Port", UintegerValue(9));
  ApplicationContainer sinkApp = serverHelper.Install(nodes.Get(1));
  sinkApp.Start(Seconds(0.0));
  sinkApp.Stop(Seconds(10.0));

  // Client (n0)
  Ptr<Socket> udpSocket = Socket::CreateSocket(nodes.Get(0), tid);
  InetSocketAddress remote = InetSocketAddress(interfaces.GetAddress(1), 9);
  Ptr<UdpClient> app = CreateObject<UdpClient>();
  app->Setup(udpSocket, remote, packetSize, numPackets, interPacketInterval, ipTos, ipTtl, enableTos, enableTtl);
  nodes.Get(0)->AddApplication(app);
  app->SetStartTime(Seconds(1.0));
  app->SetStopTime(Seconds(10.0));

  Simulator::Run();
  Simulator::Destroy();
  return 0;
}