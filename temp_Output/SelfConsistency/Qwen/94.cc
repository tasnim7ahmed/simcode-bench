#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/csma-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("Ipv6SocketOptionsCsma");

class UdpReceiver : public Application
{
public:
  UdpReceiver();
  virtual ~UdpReceiver();

  static TypeId GetTypeId(void);

  void SetLocal(Ipv6Address ip, uint16_t port);

protected:
  virtual void StartApplication(void);
  virtual void StopApplication(void);

private:
  void HandleRead(Ptr<Socket> socket);

  Ipv6Address m_localIp;
  uint16_t m_localPort;
  Ptr<Socket> m_socket;
};

UdpReceiver::UdpReceiver()
    : m_localPort(0), m_socket(nullptr)
{
}

UdpReceiver::~UdpReceiver()
{
  m_socket = nullptr;
}

TypeId
UdpReceiver::GetTypeId(void)
{
  static TypeId tid = TypeId("UdpReceiver")
                          .SetParent<Application>()
                          .SetGroupName("Tutorial");
  return tid;
}

void
UdpReceiver::SetLocal(Ipv6Address ip, uint16_t port)
{
  m_localIp = ip;
  m_localPort = port;
}

void
UdpReceiver::StartApplication(void)
{
  if (!m_socket)
  {
    TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
    m_socket = Socket::CreateSocket(GetNode(), tid);
    Inet6SocketAddress local(m_localIp, m_localPort);
    if (m_socket->Bind(local) == -1)
    {
      NS_FATAL_ERROR("Failed to bind socket");
    }

    // Enable receiving TCLASS and HOPLIMIT
    m_socket->SetRecvTClass(true);
    m_socket->SetRecvHopLimit(true);
  }

  m_socket->SetRecvCallback(MakeCallback(&UdpReceiver::HandleRead, this));
}

void
UdpReceiver::StopApplication(void)
{
  if (m_socket)
  {
    m_socket->SetRecvCallback(MakeNullCallback<void, Ptr<Socket>>());
  }
}

void
UdpReceiver::HandleRead(Ptr<Socket> socket)
{
  Ptr<Packet> packet;
  Address from;
  while ((packet = socket->RecvFrom(from)))
  {
    Ipv6Address srcAddr = Inet6SocketAddress::ConvertFrom(from).GetIpv6();
    uint8_t tclass = socket->GetLastTClass();
    uint8_t hoplimit = socket->GetLastHopLimit();

    NS_LOG_UNCOND("Received packet from " << srcAddr << " with TCLASS=" << (int)tclass
                                         << ", HOPLIMIT=" << (int)hoplimit
                                         << ", size=" << packet->GetSize() << " bytes");
  }
}

class UdpSender : public Application
{
public:
  UdpSender();
  virtual ~UdpSender();

  static TypeId GetTypeId(void);

  void Setup(Ptr<Socket> socket, Address address, uint32_t packetSize, uint32_t nPackets, Time interval);

protected:
  virtual void StartApplication(void);
  virtual void StopApplication(void);

private:
  void SendPacket(void);

  Ptr<Socket> m_socket;
  Address m_peer;
  uint32_t m_packetSize;
  uint32_t m_nPackets;
  DataRate m_dataRate;
  EventId m_sendEvent;
  bool m_running;
  uint32_t m_packetsSent;
};

UdpSender::UdpSender()
    : m_socket(0),
      m_peer(),
      m_packetSize(512),
      m_nPackets(0),
      m_dataRate("1Mbps"),
      m_sendEvent(),
      m_running(false),
      m_packetsSent(0)
{
}

UdpSender::~UdpSender()
{
  m_socket = nullptr;
}

TypeId
UdpSender::GetTypeId(void)
{
  static TypeId tid = TypeId("UdpSender")
                          .SetParent<Application>()
                          .SetGroupName("Tutorial")
                          .AddConstructor<UdpSender>();
  return tid;
}

void
UdpSender::Setup(Ptr<Socket> socket, Address address, uint32_t packetSize, uint32_t nPackets, Time interval)
{
  m_socket = socket;
  m_peer = address;
  m_packetSize = packetSize;
  m_nPackets = nPackets;
  m_dataRate = DataRate((double)packetSize / interval.GetSeconds());
}

void
UdpSender::StartApplication(void)
{
  m_running = true;
  m_packetsSent = 0;
  if (m_socket == nullptr)
  {
    TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
    m_socket = Socket::CreateSocket(GetNode(), tid);
  }

  // Example socket options: set TCLASS and HOPLIMIT
  m_socket->SetTos(0x2E); // DSCP 0x2E for EF PHB
  m_socket->SetHopLimit(16);

  SendPacket();
}

void
UdpSender::StopApplication(void)
{
  m_running = false;

  if (m_sendEvent.IsRunning())
  {
    Simulator::Cancel(m_sendEvent);
  }

  if (m_socket)
  {
    m_socket->Close();
  }
}

void
UdpSender::SendPacket()
{
  Ptr<Packet> packet = Create<Packet>(m_packetSize);
  m_socket->SendTo(packet, 0, m_peer);
  m_packetsSent++;

  if (m_packetsSent < m_nPackets)
  {
    Time interval = Seconds((double)m_packetSize * 8 / m_dataRate.GetBitRate());
    m_sendEvent = Simulator::Schedule(interval, &UdpSender::SendPacket, this);
  }
}

int
main(int argc, char *argv[])
{
  uint32_t packetSize = 512;
  uint32_t numPackets = 10;
  double interval = 1.0; // seconds

  CommandLine cmd(__FILE__);
  cmd.AddValue("packetSize", "Size of UDP packets", packetSize);
  cmd.AddValue("numPackets", "Number of packets to send", numPackets);
  cmd.AddValue("interval", "Interval between packets (seconds)", interval);
  cmd.Parse(argc, argv);

  Time interPacketInterval = Seconds(interval);

  NodeContainer nodes;
  nodes.Create(2);

  CsmaHelper csma;
  csma.SetChannelAttribute("DataRate", DataRateValue(DataRate(5000000)));
  csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));

  NetDeviceContainer devices = csma.Install(nodes);

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv6AddressHelper address;
  address.SetBase(Ipv6Address("2001:db8::"), Ipv6Prefix(64));
  Ipv6InterfaceContainer interfaces = address.Assign(devices);

  uint16_t port = 9;

  // Receiver setup
  UdpReceiverHelper receiverHelper(port);
  ApplicationContainer sinkApps = receiverHelper.Install(nodes.Get(1));
  sinkApps.Start(Seconds(0.0));
  sinkApps.Stop(Seconds(10.0));

  // Sender setup
  TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
  Ptr<Socket> senderSocket = Socket::CreateSocket(nodes.Get(0), tid);
  Inet6SocketAddress receiverAddr(interfaces.GetAddress(1, 1), port);
  Ptr<UdpSender> app = CreateObject<UdpSender>();
  app->Setup(senderSocket, receiverAddr, packetSize, numPackets, interPacketInterval);
  nodes.Get(0)->AddApplication(app);
  app->SetStartTime(Seconds(1.0));
  app->SetStopTime(Seconds(11.0));

  Simulator::Run();
  Simulator::Destroy();
  return 0;
}