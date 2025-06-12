#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include <iostream>

using namespace ns3;

class UdpTosTtlReceiver : public Application
{
public:
  UdpTosTtlReceiver()
    : m_socket(0), m_recvTos(false), m_recvTtl(false)
  {}

  void Setup(Address listenAddress, bool recvTos, bool recvTtl)
  {
    m_listenAddress = listenAddress;
    m_recvTos = recvTos;
    m_recvTtl = recvTtl;
  }

private:
  virtual void StartApplication() override
  {
    if (InetSocketAddress::IsMatchingType(m_listenAddress))
    {
      m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
      InetSocketAddress local = InetSocketAddress::ConvertFrom(m_listenAddress);
      m_socket->Bind(local);
      
      if (m_recvTos)
      {
        int optval = 1;
        m_socket->SetRecvTos(optval);
      }
      if (m_recvTtl)
      {
        int optval = 1;
        m_socket->SetRecvTtl(optval);
      }
      
      m_socket->SetRecvCallback(MakeCallback(&UdpTosTtlReceiver::HandleRead, this));
    }
  }

  virtual void StopApplication() override
  {
    if (m_socket)
    {
      m_socket->Close();
      m_socket = 0;
    }
  }

  void HandleRead(Ptr<Socket> socket)
  {
    Address from;
    while (Ptr<Packet> packet = socket->RecvFrom(from))
    {
      SocketIpTosTag tosTag;
      SocketIpTtlTag ttlTag;
      uint8_t tos = 0, ttl = 0;
      if (m_recvTos && packet->PeekPacketTag(tosTag))
      {
        tos = tosTag.GetTos();
      }
      if (m_recvTtl && packet->PeekPacketTag(ttlTag))
      {
        ttl = ttlTag.GetTtl();
      }
      NS_LOG_UNCOND("Received " << packet->GetSize()
        << " bytes from " << InetSocketAddress::ConvertFrom(from).GetIpv4()
        << " TOS: " << unsigned(tos)
        << " TTL: " << unsigned(ttl));
    }
  }

  Ptr<Socket> m_socket;
  Address m_listenAddress;
  bool m_recvTos;
  bool m_recvTtl;
};

class UdpTosTtlSender : public Application
{
public:
  UdpTosTtlSender()
    : m_socket(0), m_peerAddress(), m_packetSize(1024), m_nPackets(1), m_interval(Seconds(1.0)),
      m_sent(0), m_tos(0), m_ttl(0), m_setTos(false), m_setTtl(false)
  {}

  void Setup(Address peerAddress, uint32_t pktSize, uint32_t nPkts, Time interval, bool setTos, uint8_t tos, bool setTtl, uint8_t ttl)
  {
    m_peerAddress = peerAddress;
    m_packetSize = pktSize;
    m_nPackets = nPkts;
    m_interval = interval;
    m_setTos = setTos;
    m_tos = tos;
    m_setTtl = setTtl;
    m_ttl = ttl;
  }

private:
  virtual void StartApplication() override
  {
    m_sent = 0;
    m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
    if (m_setTos) {
      m_socket->SetIpTos(m_tos);
    }
    if (m_setTtl) {
      m_socket->SetIpTtl(m_ttl);
    }
    m_socket->Connect(m_peerAddress);
    SendPacket();
  }

  virtual void StopApplication() override
  {
    if (m_socket)
    {
      m_socket->Close();
      m_socket = 0;
    }
  }

  void SendPacket()
  {
    Ptr<Packet> packet = Create<Packet>(m_packetSize);
    m_socket->Send(packet);
    m_sent++;
    if (m_sent < m_nPackets)
    {
      Simulator::Schedule(m_interval, &UdpTosTtlSender::SendPacket, this);
    }
  }

  Ptr<Socket> m_socket;
  Address m_peerAddress;
  uint32_t m_packetSize;
  uint32_t m_nPackets;
  Time m_interval;
  uint32_t m_sent;
  uint8_t m_tos;
  uint8_t m_ttl;
  bool m_setTos;
  bool m_setTtl;
};

int main(int argc, char *argv[])
{
  uint32_t packetSize = 1024;
  uint32_t nPackets = 10;
  double interval = 1.0;
  uint8_t tos = 0;
  uint8_t ttl = 64;
  bool setTos = false;
  bool setTtl = false;
  bool recvTos = false;
  bool recvTtl = false;

  CommandLine cmd;
  cmd.AddValue("packetSize", "Size of each packet [bytes]", packetSize);
  cmd.AddValue("nPackets", "Number of packets to send", nPackets);
  cmd.AddValue("interval", "Interval between packets [s]", interval);
  cmd.AddValue("tos", "IP_TOS value to set for outgoing packets", tos);
  cmd.AddValue("ttl", "IP_TTL value to set for outgoing packets", ttl);
  cmd.AddValue("setTos", "Set IP_TOS on sender side", setTos);
  cmd.AddValue("setTtl", "Set IP_TTL on sender side", setTtl);
  cmd.AddValue("recvTos", "Receiver requests TOS tag (sets SO_RECVTOS)", recvTos);
  cmd.AddValue("recvTtl", "Receiver requests TTL tag (sets SO_RECVTTL)", recvTtl);
  cmd.Parse(argc, argv);

  LogComponentEnable("UdpTosTtlReceiver", LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create(2);

  CsmaHelper csma;
  csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
  csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));

  NetDeviceContainer devices = csma.Install(nodes);

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  uint16_t port = 9999;
  Address receiverAddress (InetSocketAddress(interfaces.GetAddress(1), port));

  Ptr<UdpTosTtlSender> senderApp = CreateObject<UdpTosTtlSender>();
  senderApp->Setup(receiverAddress, packetSize, nPackets, Seconds(interval), setTos, tos, setTtl, ttl);
  nodes.Get(0)->AddApplication(senderApp);
  senderApp->SetStartTime(Seconds(1.0));
  senderApp->SetStopTime(Seconds(2.0 + interval * nPackets));

  Ptr<UdpTosTtlReceiver> receiverApp = CreateObject<UdpTosTtlReceiver>();
  receiverApp->Setup(InetSocketAddress(Ipv4Address::GetAny(), port), recvTos, recvTtl);
  nodes.Get(1)->AddApplication(receiverApp);
  receiverApp->SetStartTime(Seconds(0.0));
  receiverApp->SetStopTime(Seconds(3.0 + interval * nPackets));

  Simulator::Stop(Seconds(4.0 + interval * nPackets));
  Simulator::Run();
  Simulator::Destroy();
  return 0;
}