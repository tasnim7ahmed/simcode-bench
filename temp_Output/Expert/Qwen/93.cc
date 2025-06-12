#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("UdpTosTtlExample");

class UdpReceiver : public Application
{
public:
  static TypeId GetTypeId(void)
  {
    static TypeId tid = TypeId("UdpReceiver")
      .SetParent<Application>()
      .AddConstructor<UdpReceiver>()
      .AddAttribute("LocalPort", "The local port to listen on",
                    UintegerValue(9),
                    MakeUintegerAccessor(&UdpReceiver::m_port),
                    MakeUintegerChecker<uint16_t>());
    return tid;
  }

  UdpReceiver() {}
  virtual ~UdpReceiver() {}

private:
  virtual void StartApplication()
  {
    m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
    InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), m_port);
    m_socket->Bind(local);
    m_socket->SetRecvCallback(MakeCallback(&UdpReceiver::Receive, this));
  }

  virtual void StopApplication()
  {
    if (m_socket)
    {
      m_socket->Close();
    }
  }

  void Receive(Ptr<Socket> socket)
  {
    Ptr<Packet> packet;
    Address from;
    while ((packet = socket->RecvFrom(from)))
    {
      Ipv4Header ipHdr;
      socket->GetSockName(from);
      packet->PeekHeader(ipHdr);

      uint8_t tos = ipHdr.GetTos();
      uint8_t ttl = ipHdr.GetTtl();

      NS_LOG_UNCOND("Received packet with size " << packet->GetSize() << ", TOS: " << (uint32_t)tos << ", TTL: " << (uint32_t)ttl);
    }
  }

  uint16_t m_port;
  Ptr<Socket> m_socket;
};

class UdpSender : public Application
{
public:
  static TypeId GetTypeId(void)
  {
    static TypeId tid = TypeId("UdpSender")
      .SetParent<Application>()
      .AddConstructor<UdpSender>()
      .AddAttribute("RemoteAddress", "The destination IP address",
                    Ipv4AddressValue("10.1.1.2"),
                    MakeIpv4AddressAccessor(&UdpSender::m_remoteAddress),
                    MakeIpv4AddressChecker())
      .AddAttribute("RemotePort", "The destination port",
                    UintegerValue(9),
                    MakeUintegerAccessor(&UdpSender::m_remotePort),
                    MakeUintegerChecker<uint16_t>())
      .AddAttribute("PacketSize", "Size of the packets",
                    UintegerValue(512),
                    MakeUintegerAccessor(&UdpSender::m_size),
                    MakeUintegerChecker<uint32_t>())
      .AddAttribute("NumPackets", "Number of packets to send",
                    UintegerValue(5),
                    MakeUintegerAccessor(&UdpSender::m_count),
                    MakeUintegerChecker<uint32_t>())
      .AddAttribute("Interval", "Time between packets",
                    TimeValue(Seconds(1.0)),
                    MakeTimeAccessor(&UdpSender::m_interval),
                    MakeTimeChecker())
      .AddAttribute("Tos", "IP_TOS value for packets",
                    UintegerValue(0),
                    MakeUintegerAccessor(&UdpSender::m_tos),
                    MakeUintegerChecker<uint8_t>())
      .AddAttribute("Ttl", "IP_TTL value for packets",
                    UintegerValue(64),
                    MakeUintegerAccessor(&UdpSender::m_ttl),
                    MakeUintegerChecker<uint8_t>());
    return tid;
  }

  UdpSender() : m_sent(0) {}
  virtual ~UdpSender() {}

private:
  virtual void StartApplication()
  {
    m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
    m_socket->SetAllowBroadcast(true);
    m_socket->SetIpTos(m_tos);
    m_socket->SetIpTtl(m_ttl);

    m_sendEvent = Simulator::ScheduleNow(&UdpSender::Send, this);
  }

  virtual void StopApplication()
  {
    if (m_sendEvent.IsRunning())
    {
      Simulator::Cancel(m_sendEvent);
    }

    if (m_socket)
    {
      m_socket->Close();
    }
  }

  void Send()
  {
    if (m_sent < m_count)
    {
      Ptr<Packet> packet = Create<Packet>(m_size);
      m_socket->SendTo(packet, 0, InetSocketAddress(m_remoteAddress, m_remotePort));
      NS_LOG_UNCOND("Sent packet " << m_sent + 1 << " with TOS=" << (uint32_t)m_tos << ", TTL=" << (uint32_t)m_ttl);
      m_sent++;
      m_sendEvent = Simulator::Schedule(m_interval, &UdpSender::Send, this);
    }
  }

  Ipv4Address m_remoteAddress;
  uint16_t m_remotePort;
  uint32_t m_size;
  uint32_t m_count;
  uint32_t m_sent;
  Time m_interval;
  uint8_t m_tos;
  uint8_t m_ttl;
  EventId m_sendEvent;
  Ptr<Socket> m_socket;
};

int main(int argc, char *argv[])
{
  uint32_t packetSize = 512;
  uint32_t numPackets = 5;
  double interval = 1.0;
  uint8_t tos = 0;
  uint8_t ttl = 64;
  bool enableRecvTos = false;
  bool enableRecvTtl = false;

  CommandLine cmd(__FILE__);
  cmd.AddValue("packetSize", "Size of each packet in bytes", packetSize);
  cmd.AddValue("numPackets", "Number of packets to send", numPackets);
  cmd.AddValue("interval", "Interval between packets in seconds", interval);
  cmd.AddValue("tos", "IP_TOS value for packets sent", tos);
  cmd.AddValue("ttl", "IP_TTL value for packets sent", ttl);
  cmd.AddValue("RECVTOS", "Enable receiving TOS values at receiver", enableRecvTos);
  cmd.AddValue("RECVTTL", "Enable receiving TTL values at receiver", enableRecvTtl);
  cmd.Parse(argc, argv);

  Time interPacketInterval = Seconds(interval);

  NodeContainer nodes;
  nodes.Create(2);

  CsmaHelper csma;
  csma.SetChannelAttribute("DataRate", DataRateValue(DataRate("100Mbps")));
  csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));

  NetDeviceContainer devices = csma.Install(nodes);

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  UdpReceiverHelper receiverHelper;
  ApplicationContainer sinkApp = receiverHelper.Install(nodes.Get(1));
  sinkApp.Start(Seconds(0.0));
  sinkApp.Stop(Seconds(10.0));

  UdpSenderHelper senderHelper;
  senderHelper.SetAttribute("RemoteAddress", Ipv4AddressValue(interfaces.GetAddress(1)));
  senderHelper.SetAttribute("RemotePort", UintegerValue(9));
  senderHelper.SetAttribute("PacketSize", UintegerValue(packetSize));
  senderHelper.SetAttribute("NumPackets", UintegerValue(numPackets));
  senderHelper.SetAttribute("Interval", TimeValue(interPacketInterval));
  senderHelper.SetAttribute("Tos", UintegerValue(tos));
  senderHelper.SetAttribute("Ttl", UintegerValue(ttl));

  ApplicationContainer sourceApp = senderHelper.Install(nodes.Get(0));
  sourceApp.Start(Seconds(0.5));
  sourceApp.Stop(Seconds(10.0));

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  Simulator::Run();
  Simulator::Destroy();
  return 0;
}