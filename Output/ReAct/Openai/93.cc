#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-header.h"
#include "ns3/ipv4-raw-socket-factory.h"
#include <iostream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("CsmaUdpTosTtl");

class UdpSenderApp : public Application
{
public:
  UdpSenderApp();
  virtual ~UdpSenderApp();
  void Setup(Address address, uint32_t packetSize, uint32_t nPackets, Time pktInterval, uint8_t tos, uint8_t ttl);

private:
  virtual void StartApplication(void);
  virtual void StopApplication(void);
  void SendPacket(void);

  Ptr<Socket>     m_socket;
  Address         m_peer;
  uint32_t        m_packetSize;
  uint32_t        m_nPackets;
  Time            m_pktInterval;
  uint32_t        m_packetsSent;
  EventId         m_sendEvent;
  uint8_t         m_tos;
  uint8_t         m_ttl;
};

UdpSenderApp::UdpSenderApp()
  : m_socket(0),
    m_peer(),
    m_packetSize(0),
    m_nPackets(0),
    m_pktInterval(Seconds(1.0)),
    m_packetsSent(0),
    m_tos(0),
    m_ttl(64)
{
}

UdpSenderApp::~UdpSenderApp()
{
  m_socket = 0;
}

void
UdpSenderApp::Setup(Address address, uint32_t packetSize, uint32_t nPackets, Time pktInterval, uint8_t tos, uint8_t ttl)
{
  m_peer = address;
  m_packetSize = packetSize;
  m_nPackets = nPackets;
  m_pktInterval = pktInterval;
  m_tos = tos;
  m_ttl = ttl;
}

void
UdpSenderApp::StartApplication(void)
{
  if(!m_socket)
  {
    m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
    m_socket->SetIpTos(m_tos);
    m_socket->SetIpTtl(m_ttl);
    m_socket->Connect(m_peer);
  }
  m_packetsSent = 0;
  SendPacket();
}

void
UdpSenderApp::StopApplication(void)
{
  if(m_socket)
  {
    m_socket->Close();
  }
}

void
UdpSenderApp::SendPacket(void)
{
  Ptr<Packet> packet = Create<Packet>(m_packetSize);
  m_socket->Send(packet);
  ++m_packetsSent;

  if(m_packetsSent < m_nPackets)
  {
    m_sendEvent = Simulator::Schedule(m_pktInterval, &UdpSenderApp::SendPacket, this);
  }
}

class UdpReceiverApp : public Application
{
public:
  UdpReceiverApp();
  virtual ~UdpReceiverApp();
  void Setup(Address address, bool recvTos, bool recvTtl);

private:
  virtual void StartApplication(void);
  virtual void StopApplication(void);
  void HandleRead(Ptr<Socket> socket);

  Ptr<Socket>     m_socket;
  Address         m_local;
  bool            m_recvTos;
  bool            m_recvTtl;
};

UdpReceiverApp::UdpReceiverApp()
  : m_socket(0),
    m_local(),
    m_recvTos(false),
    m_recvTtl(false)
{
}

UdpReceiverApp::~UdpReceiverApp()
{
  m_socket = 0;
}

void
UdpReceiverApp::Setup(Address address, bool recvTos, bool recvTtl)
{
  m_local = address;
  m_recvTos = recvTos;
  m_recvTtl = recvTtl;
}

void
UdpReceiverApp::StartApplication(void)
{
  if(!m_socket)
  {
    m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
    m_socket->Bind(m_local);
    if (m_recvTos)
    {
      int optval = 1;
      m_socket->SetSockOpt(SOCKET_IP_LEVEL, NS3_SO_RECVTOS, &optval, sizeof(optval));
    }
    if (m_recvTtl)
    {
      int optval = 1;
      m_socket->SetSockOpt(SOCKET_IP_LEVEL, NS3_SO_RECVTTL, &optval, sizeof(optval));
    }
  }
  m_socket->SetRecvCallback(MakeCallback(&UdpReceiverApp::HandleRead, this));
}

void
UdpReceiverApp::StopApplication(void)
{
  if(m_socket)
  {
    m_socket->Close();
  }
}

void
UdpReceiverApp::HandleRead(Ptr<Socket> socket)
{
  Address from;
  while (Ptr<Packet> packet = socket->RecvFrom(from))
  {
    SocketIpTosTag tosTag;
    SocketIpTtlTag ttlTag;
    uint8_t tos = 0;
    uint8_t ttl = 0;

    if (packet->PeekPacketTag(tosTag))
      tos = tosTag.GetTos();
    if (packet->PeekPacketTag(ttlTag))
      ttl = ttlTag.GetTtl();

    std::cout << "At " << Simulator::Now().GetSeconds()
              << "s received " << packet->GetSize() << " bytes from "
              << InetSocketAddress::ConvertFrom(from).GetIpv4()
              << " port " << InetSocketAddress::ConvertFrom(from).GetPort();

    if (m_recvTos)
      std::cout << " TOS=" << unsigned(tos);
    if (m_recvTtl)
      std::cout << " TTL=" << unsigned(ttl);

    std::cout << std::endl;
  }
}

int main(int argc, char *argv[])
{
  uint32_t packetSize = 1024;
  uint32_t numPackets = 10;
  double interval = 1.0;
  uint32_t tos = 0;
  uint32_t ttl = 64;
  bool recvTos = false;
  bool recvTtl = false;

  CommandLine cmd(__FILE__);
  cmd.AddValue("packetSize", "Packet size in bytes", packetSize);
  cmd.AddValue("numPackets", "Number of packets to send", numPackets);
  cmd.AddValue("interval", "Interval between packets (in seconds)", interval);
  cmd.AddValue("tos", "Set IP_TOS (Type-Of-Service) value in packet", tos);
  cmd.AddValue("ttl", "Set IP_TTL (Time-To-Live) value in packet", ttl);
  cmd.AddValue("recvTos", "If true, receiver enables SO_RECVTOS", recvTos);
  cmd.AddValue("recvTtl", "If true, receiver enables SO_RECVTTL", recvTtl);
  cmd.Parse(argc, argv);

  NodeContainer nodes;
  nodes.Create(2);

  CsmaHelper csma;
  csma.SetChannelAttribute("DataRate", DataRateValue(DataRate("100Mbps")));
  csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));

  NetDeviceContainer devices = csma.Install(nodes);

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  uint16_t port = 8000;
  Address recvAddress(InetSocketAddress(interfaces.GetAddress(1), port));

  Ptr<UdpSenderApp> sender = CreateObject<UdpSenderApp>();
  sender->Setup(recvAddress, packetSize, numPackets, Seconds(interval), tos, ttl);
  nodes.Get(0)->AddApplication(sender);
  sender->SetStartTime(Seconds(1.0));
  sender->SetStopTime(Seconds(1.0 + numPackets * interval + 1.0));

  Address bindAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
  Ptr<UdpReceiverApp> receiver = CreateObject<UdpReceiverApp>();
  receiver->Setup(bindAddress, recvTos, recvTtl);
  nodes.Get(1)->AddApplication(receiver);
  receiver->SetStartTime(Seconds(0.0));
  receiver->SetStopTime(Seconds(1.0 + numPackets * interval + 2.0));

  Simulator::Stop(Seconds(1.0 + numPackets * interval + 3.0));
  Simulator::Run();
  Simulator::Destroy();
  return 0;
}