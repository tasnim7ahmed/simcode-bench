#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv6-static-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("UdpIpv6TclassHoplimit");

class UdpReceiver : public Application
{
public:
  static TypeId GetTypeId(void)
  {
    TypeId tid = TypeId("UdpReceiver")
      .SetParent<Application>()
      .AddConstructor<UdpReceiver>()
      .AddAttribute("LocalPort", "Port on which the receiver listens",
                    UintegerValue(9),
                    MakeUintegerAccessor(&UdpReceiver::m_port),
                    MakeUintegerChecker<uint16_t>());
    return tid;
  }

  UdpReceiver() {}
  virtual ~UdpReceiver() {}

private:
  void StartApplication(void)
  {
    TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
    Ptr<Socket> socket = Socket::CreateSocket(GetNode(), tid);
    InetSocketAddress local = InetSocketAddress(Ipv6Address::GetAny(), m_port);
    socket->Bind(local);
    socket->SetRecvCallback(MakeCallback(&UdpReceiver::HandleRead, this));
    socket->SetAttribute("RecvTClass", BooleanValue(true));
    socket->SetAttribute("RecvHopLimit", BooleanValue(true));
  }

  void HandleRead(Ptr<Socket> socket)
  {
    Ptr<Packet> packet;
    Address from;
    while ((packet = socket->RecvFrom(from)))
    {
      Ipv6Header ipHeader;
      socket->GetIpRecvTos();
      socket->GetIpRecvHopLimit();
      uint8_t tclass = socket->GetIpRecvTos();
      uint8_t hoplimit = socket->GetIpRecvHopLimit();

      if (packet->PeekHeader(ipHeader))
      {
        NS_LOG_UNCOND("Received packet: size=" << packet->GetSize()
                                              << " TCLASS=" << static_cast<uint32_t>(tclass)
                                              << " HOPLIMIT=" << static_cast<uint32_t>(hoplimit));
      }
    }
  }

  void StopApplication(void) {}

  uint16_t m_port;
};

class UdpSender : public Application
{
public:
  static TypeId GetTypeId(void)
  {
    TypeId tid = TypeId("UdpSender")
      .SetParent<Application>()
      .AddConstructor<UdpSender>()
      .AddAttribute("RemoteAddress", "The destination IPv6 address",
                    Ipv6AddressValue(),
                    MakeIpv6Accessor(&UdpSender::m_destAddr),
                    MakeIpv6Checker())
      .AddAttribute("RemotePort", "The destination port",
                    UintegerValue(9),
                    MakeUintegerAccessor(&UdpSender::m_destPort),
                    MakeUintegerChecker<uint16_t>())
      .AddAttribute("PacketSize", "Size of packets to send (bytes)",
                    UintegerValue(512),
                    MakeUintegerAccessor(&UdpSender::m_size),
                    MakeUintegerChecker<uint32_t>())
      .AddAttribute("NumPackets", "Number of packets to send",
                    UintegerValue(5),
                    MakeUintegerAccessor(&UdpSender::m_count),
                    MakeUintegerChecker<uint32_t>())
      .AddAttribute("Interval", "Time between packets (seconds)",
                    TimeValue(Seconds(1.0)),
                    MakeTimeAccessor(&UdpSender::m_interval),
                    MakeTimeChecker())
      .AddAttribute("TClass", "Traffic Class for outgoing packets",
                    UintegerValue(0),
                    MakeUintegerAccessor(&UdpSender::m_tclass),
                    MakeUintegerChecker<uint8_t>())
      .AddAttribute("HopLimit", "Hop Limit for outgoing packets",
                    UintegerValue(64),
                    MakeUintegerAccessor(&UdpSender::m_hoplimit),
                    MakeUintegerChecker<uint8_t>());
    return tid;
  }

  UdpSender() : m_sent(0) {}
  virtual ~UdpSender() {}

private:
  void StartApplication(void)
  {
    TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
    Ptr<Socket> socket = Socket::CreateSocket(GetNode(), tid);
    socket->SetAttribute("IpTos", UintegerValue(m_tclass));
    socket->SetAttribute("IpHopLimit", UintegerValue(m_hoplimit));

    m_sendEvent = Simulator::ScheduleNow(&UdpSender::SendPacket, this);
  }

  void SendPacket(void)
  {
    Ptr<Packet> packet = Create<Packet>(m_size);
    Address dest = InetSocketAddress(m_destAddr, m_destPort);
    if (m_sent < m_count)
    {
      if (m_socket->SendTo(packet, 0, dest) >= 0)
      {
        NS_LOG_UNCOND("Sent packet " << m_sent + 1 << " with TCLASS=" << static_cast<uint32_t>(m_tclass)
                                    << " and HOPLIMIT=" << static_cast<uint32_t>(m_hoplimit));
        m_sent++;
        m_sendEvent = Simulator::Schedule(m_interval, &UdpSender::SendPacket, this);
      }
    }
  }

  void StopApplication(void)
  {
    Simulator::Cancel(m_sendEvent);
  }

  Ipv6Address m_destAddr;
  uint16_t m_destPort;
  uint32_t m_size;
  uint32_t m_count;
  Time m_interval;
  uint8_t m_tclass;
  uint8_t m_hoplimit;
  uint32_t m_sent;
  EventId m_sendEvent;
  Ptr<Socket> m_socket;
};

int main(int argc, char *argv[])
{
  uint32_t packetSize = 512;
  uint32_t numPackets = 5;
  double interval = 1.0;
  uint8_t tclass = 0;
  uint8_t hoplimit = 64;

  CommandLine cmd(__FILE__);
  cmd.AddValue("packetSize", "Size of packets in bytes", packetSize);
  cmd.AddValue("numPackets", "Number of packets to send", numPackets);
  cmd.AddValue("interval", "Interval between packets in seconds", interval);
  cmd.AddValue("tclass", "IPv6 Traffic Class value", tclass);
  cmd.AddValue("hoplimit", "IPv6 Hop Limit value", hoplimit);
  cmd.Parse(argc, argv);

  NodeContainer nodes;
  nodes.Create(2);

  CsmaHelper csma;
  csma.SetChannelAttribute("DataRate", DataRateValue(DataRate("100Mbps")));
  csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));

  NetDeviceContainer devices = csma.Install(nodes);

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv6AddressHelper address;
  address.SetBase(Ipv6Address("2001:db8::"), Ipv6Prefix(64));
  Ipv6InterfaceContainer interfaces = address.Assign(devices);

  UdpReceiverHelper receiver(nodes.Get(1), 9);
  ApplicationContainer rcvApp = receiver.Install(nodes.Get(1));
  rcvApp.Start(Seconds(0.0));
  rcvApp.Stop(Seconds(10.0));

  UdpSenderHelper sender(nodes.Get(0), interfaces.GetAddress(1, 1), 9);
  sender.SetAttribute("PacketSize", UintegerValue(packetSize));
  sender.SetAttribute("NumPackets", UintegerValue(numPackets));
  sender.SetAttribute("Interval", TimeValue(Seconds(interval)));
  sender.SetAttribute("TClass", UintegerValue(tclass));
  sender.SetAttribute("HopLimit", UintegerValue(hoplimit));

  ApplicationContainer sndApp = sender.Install(nodes.Get(0));
  sndApp.Start(Seconds(0.5));
  sndApp.Stop(Seconds(10.0));

  Simulator::Run();
  Simulator::Destroy();

  return 0;
}