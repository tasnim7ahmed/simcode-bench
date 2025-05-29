#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("Ipv6UdpTclassHoplimitExample");

class UdpIpv6CustomReceiver : public Application
{
public:
  UdpIpv6CustomReceiver() : m_socket(0) {}
  virtual ~UdpIpv6CustomReceiver() { m_socket = 0; }

  void Setup(Address address, uint16_t port)
  {
    m_local = Inet6SocketAddress(Ipv6Address::ConvertFrom(address), port);
    m_port = port;
  }

private:
  virtual void StartApplication()
  {
    if (!m_socket)
    {
      m_socket = Socket::CreateSocket(GetNode(), TypeId::LookupByName("ns3::UdpSocketFactory"));
      m_socket->SetRecvRecvTClass(true);
      m_socket->SetRecvHopLimit(true);
      m_socket->Bind(m_local);
    }
    m_socket->SetRecvCallback(MakeCallback(&UdpIpv6CustomReceiver::HandleRead, this));
  }

  virtual void StopApplication()
  {
    if (m_socket)
    {
      m_socket->SetRecvCallback(MakeNullCallback<void, Ptr<Socket> >());
      m_socket->Close();
    }
  }

  void HandleRead(Ptr<Socket> socket)
  {
    Address from;
    Ptr<Packet> packet;
    while ((packet = socket->RecvFrom(from)))
    {
      SocketIpTosTag tcTag;
      SocketIpTclassTag tclassTag;
      SocketIpv6HopLimitTag hopLimitTag;
      uint8_t tclass = 0;
      uint8_t hoplimit = 0;
      bool foundTclass = false, foundHoplimit = false;

      if (packet->PeekPacketTag(tclassTag))
      {
        tclass = tclassTag.GetTclass();
        foundTclass = true;
      }
      else if (packet->PeekPacketTag(tcTag))
      {
        tclass = tcTag.GetTos();
        foundTclass = true;
      }

      if (packet->PeekPacketTag(hopLimitTag))
      {
        hoplimit = hopLimitTag.GetHopLimit();
        foundHoplimit = true;
      }

      NS_LOG_UNCOND("n1 received packet of size " << packet->GetSize()
        << ", from " << Inet6SocketAddress::ConvertFrom(from).GetIpv6()
        << ", TCLASS=" << (foundTclass ? std::to_string((uint32_t)tclass) : "N/A")
        << ", HOPLIMIT=" << (foundHoplimit ? std::to_string((uint32_t)hoplimit) : "N/A")
      );
    }
  }

  Ptr<Socket> m_socket;
  Inet6SocketAddress m_local;
  uint16_t m_port;
};

class UdpIpv6CustomSender : public Application
{
public:
  UdpIpv6CustomSender()
    : m_socket(0), m_sendEvent(), m_running(false),
      m_packetsSent(0), m_nPackets(1), m_packetSize(512), m_interval(Seconds(1.0)),
      m_tclass(-1), m_hoplimit(-1) {}

  virtual ~UdpIpv6CustomSender() { m_socket = 0; }

  void Setup(Address address, uint16_t port, uint32_t pktSize, uint32_t nPkts, Time interval, int32_t tclass, int32_t hoplimit)
  {
    m_peer = Inet6SocketAddress(Ipv6Address::ConvertFrom(address), port);
    m_packetSize = pktSize;
    m_nPackets = nPkts;
    m_interval = interval;
    m_tclass = tclass;
    m_hoplimit = hoplimit;
  }

private:
  virtual void StartApplication()
  {
    m_running = true;
    m_packetsSent = 0;
    m_socket = Socket::CreateSocket(GetNode(), TypeId::LookupByName("ns3::UdpSocketFactory"));
    if (m_tclass >= 0)
    {
      m_socket->SetIpTclass(m_tclass);
    }
    if (m_hoplimit >= 0)
    {
      m_socket->SetIpMulticastTtl(m_hoplimit); // fallback, but set below if possible
      m_socket->SetIpv6HopLimit(m_hoplimit);
    }
    m_socket->Connect(m_peer);
    SendPacket();
  }

  virtual void StopApplication()
  {
    m_running = false;
    if (m_sendEvent.IsRunning())
      Simulator::Cancel(m_sendEvent);
    if (m_socket)
      m_socket->Close();
  }

  void SendPacket()
  {
    Ptr<Packet> packet = Create<Packet>(m_packetSize);
    m_socket->Send(packet);
    ++m_packetsSent;
    if (m_packetsSent < m_nPackets && m_running)
      m_sendEvent = Simulator::Schedule(m_interval, &UdpIpv6CustomSender::SendPacket, this);
  }

  Ptr<Socket> m_socket;
  EventId m_sendEvent;
  bool m_running;
  uint32_t m_packetsSent;
  uint32_t m_nPackets;
  uint32_t m_packetSize;
  Time m_interval;
  Inet6SocketAddress m_peer;
  int32_t m_tclass;
  int32_t m_hoplimit;
};

int main(int argc, char *argv[])
{
  uint32_t packetSize = 512;
  uint32_t numPackets = 10;
  double interval = 1.0;
  int32_t tclass = -1;
  int32_t hoplimit = -1;

  CommandLine cmd(__FILE__);
  cmd.AddValue("packetSize", "Packet size in bytes", packetSize);
  cmd.AddValue("numPackets", "Number of packets", numPackets);
  cmd.AddValue("interval", "Interval between packets (seconds)", interval);
  cmd.AddValue("tclass", "IPv6 Traffic Class (TCLASS) for sender, -1 to unset", tclass);
  cmd.AddValue("hoplimit", "IPv6 Hop Limit for sender, -1 to unset", hoplimit);
  cmd.Parse(argc, argv);

  NodeContainer nodes;
  nodes.Create(2);

  CsmaHelper csma;
  csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
  csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));

  NetDeviceContainer devices = csma.Install(nodes);

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv6AddressHelper ipv6;
  ipv6.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
  Ipv6InterfaceContainer ifs = ipv6.Assign(devices);
  // Enable global unicast forwarding only for completeness
  ifs.SetForwarding(0, true);
  ifs.SetForwarding(1, true);
  ifs.SetDefaultRouteInAllNodes(0);

  uint16_t port = 50000;

  // Receiver on n1
  Ptr<UdpIpv6CustomReceiver> receiverApp = CreateObject<UdpIpv6CustomReceiver>();
  receiverApp->Setup(ifs.GetAddress(1,1), port);
  nodes.Get(1)->AddApplication(receiverApp);
  receiverApp->SetStartTime(Seconds(0.0));
  receiverApp->SetStopTime(Seconds(20.0));

  // Sender on n0
  Ptr<UdpIpv6CustomSender> senderApp = CreateObject<UdpIpv6CustomSender>();
  senderApp->Setup(ifs.GetAddress(1,1), port, packetSize, numPackets, Seconds(interval), tclass, hoplimit);
  nodes.Get(0)->AddApplication(senderApp);
  senderApp->SetStartTime(Seconds(1.0));
  senderApp->SetStopTime(Seconds(20.0));

  Simulator::Stop(Seconds(21.0));
  Simulator::Run();
  Simulator::Destroy();
  return 0;
}