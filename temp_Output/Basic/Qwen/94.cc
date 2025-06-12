#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/csma-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("Ipv6UdpSocketOptionsExample");

class UdpReceiver : public Application {
public:
  UdpReceiver() {}
  virtual ~UdpReceiver() {}

  static TypeId GetTypeId(void) {
    static TypeId tid = TypeId("UdpReceiver")
      .SetParent<Application>()
      .AddConstructor<UdpReceiver>();
    return tid;
  }

  void Setup(Ptr<Socket> socket, uint16_t port) {
    m_socket = socket;
    m_localPort = port;
  }

protected:
  void DoStart(void) override {
    m_socket->Bind(Inet6SocketAddress(Ipv6Address::GetAny(), m_localPort));
    m_socket->SetRecvCallback(MakeCallback(&UdpReceiver::HandleRead, this));
    Application::DoStart();
  }

  void HandleRead(Ptr<Socket> socket) {
    Ptr<Packet> packet;
    Address from;
    while ((packet = socket->RecvFrom(from))) {
      Ipv6Address srcAddr = Inet6SocketAddress::ConvertFrom(from).GetIpv6();
      uint16_t srcPort = Inet6SocketAddress::ConvertFrom(from).GetPort();

      SocketIpv6TClassTag tclassTag;
      SocketIpv6HopLimitTag hoplimitTag;

      uint8_t tclass = 0;
      uint8_t hoplimit = 0;

      if (packet->RemovePacketTag(tclassTag)) {
        tclass = tclassTag.GetTClass();
      }
      if (packet->RemovePacketTag(hoplimitTag)) {
        hoplimit = hoplimitTag.GetHopLimit();
      }

      NS_LOG_UNCOND("Received packet at node " << socket->GetNode()->GetId()
                    << " from " << srcAddr << ":" << srcPort
                    << " with TCLASS=" << (uint32_t)tclass
                    << ", HOPLIMIT=" << (uint32_t)hoplimit);
    }
  }

private:
  Ptr<Socket> m_socket;
  uint16_t m_localPort;
};

class UdpSender : public Application {
public:
  UdpSender() {}
  virtual ~UdpSender() {}

  static TypeId GetTypeId(void) {
    static TypeId tid = TypeId("UdpSender")
      .SetParent<Application>()
      .AddConstructor<UdpSender>()
      .AddAttribute("RemoteAddress", "The destination IPv6 address",
                    Ipv6AddressValue(),
                    MakeIpv6AddressAccessor(&UdpSender::m_peerAddr),
                    MakeIpv6AddressChecker())
      .AddAttribute("RemotePort", "The destination port",
                    UintegerValue(9),
                    MakeUintegerAccessor(&UdpSender::m_peerPort),
                    MakeUintegerChecker<uint16_t>())
      .AddAttribute("PacketSize", "Size of packets to send",
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
      .AddAttribute("TClass", "IPv6 Traffic Class value",
                    UintegerValue(0),
                    MakeUintegerAccessor(&UdpSender::m_tclass),
                    MakeUintegerChecker<uint8_t>())
      .AddAttribute("HopLimit", "IPv6 Hop Limit value",
                    UintegerValue(64),
                    MakeUintegerAccessor(&UdpSender::m_hoplimit),
                    MakeUintegerChecker<uint8_t>());
    return tid;
  }

  void Setup(Ptr<Socket> socket) {
    m_socket = socket;
  }

protected:
  void DoStart(void) override {
    m_sendEvent = Simulator::ScheduleNow(&UdpSender::SendPacket, this);
    Application::DoStart();
  }

  void SendPacket(void) {
    Ptr<Packet> packet = Create<Packet>(m_size);
    m_socket->SetIpv6Tclass(m_tclass);
    m_socket->SetIpv6HopLimit(m_hoplimit);

    m_socket->SendTo(packet, 0, Inet6SocketAddress(m_peerAddr, m_peerPort));

    if (++m_sent < m_count) {
      m_sendEvent = Simulator::Schedule(m_interval, &UdpSender::SendPacket, this);
    }
  }

private:
  Ptr<Socket> m_socket;
  Ipv6Address m_peerAddr;
  uint16_t m_peerPort;
  uint32_t m_size;
  uint32_t m_count;
  uint32_t m_sent;
  Time m_interval;
  uint8_t m_tclass;
  uint8_t m_hoplimit;
  EventId m_sendEvent;
};

int main(int argc, char *argv[]) {
  uint32_t packetSize = 512;
  uint32_t numPackets = 5;
  double interval = 1.0;
  uint8_t tclass = 0;
  uint8_t hoplimit = 64;

  CommandLine cmd(__FILE__);
  cmd.AddValue("packetSize", "Size of each packet in bytes", packetSize);
  cmd.AddValue("numPackets", "Number of packets to send", numPackets);
  cmd.AddValue("interval", "Interval between packets in seconds", interval);
  cmd.AddValue("tclass", "Traffic Class value for IPv6 header", tclass);
  cmd.AddValue("hoplimit", "Hop Limit value for IPv6 header", hoplimit);
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

  uint16_t port = 9;

  // Receiver application
  UdpReceiverHelper receiver(port);
  ApplicationContainer appReceiver = receiver.Install(nodes.Get(1));
  appReceiver.Start(Seconds(1.0));
  appReceiver.Stop(Seconds(10.0));

  // Sender application
  UdpSenderHelper sender;
  sender.SetAttribute("RemoteAddress", Ipv6AddressValue(interfaces.GetAddress(1, 1)));
  sender.SetAttribute("RemotePort", UintegerValue(port));
  sender.SetAttribute("PacketSize", UintegerValue(packetSize));
  sender.SetAttribute("NumPackets", UintegerValue(numPackets));
  sender.SetAttribute("Interval", TimeValue(Seconds(interval)));
  sender.SetAttribute("TClass", UintegerValue(tclass));
  sender.SetAttribute("HopLimit", UintegerValue(hoplimit));

  ApplicationContainer appSender = sender.Install(nodes.Get(0));
  appSender.Start(Seconds(2.0));
  appSender.Stop(Seconds(10.0));

  // Enable RECVTCLASS and RECVHOPLIMIT on receiver socket
  Config::Set("/NodeList/1/ApplicationList/0/$ns3::UdpReceiver/Socket/Ipv6RecvTclass", BooleanValue(true));
  Config::Set("/NodeList/1/ApplicationList/0/$ns3::UdpReceiver/Socket/Ipv6RecvHopLimit", BooleanValue(true));

  Simulator::Run();
  Simulator::Destroy();
  return 0;
}