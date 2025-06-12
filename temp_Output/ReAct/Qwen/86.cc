#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("CustomWifiApp");

class SenderApplication : public Application
{
public:
  static TypeId GetTypeId(void)
  {
    static TypeId tid = TypeId("SenderApplication")
                            .SetParent<Application>()
                            .SetGroupName("Tutorial")
                            .AddConstructor<SenderApplication>()
                            .AddAttribute("PacketSize",
                                          "Size of packets sent in bytes.",
                                          UintegerValue(1024),
                                          MakeUintegerAccessor(&SenderApplication::m_packetSize),
                                          MakeUintegerChecker<uint32_t>())
                            .AddAttribute("DestinationAddress",
                                          "Destination MAC address for the packets.",
                                          Mac48AddressValue(Mac48Address("00:00:00:00:00:01")),
                                          MakeMac48AddressAccessor(&SenderApplication::m_destMac),
                                          MakeMac48AddressChecker())
                            .AddAttribute("Port",
                                          "Destination port for sending packets.",
                                          UintegerValue(9),
                                          MakeUintegerAccessor(&SenderApplication::m_port),
                                          MakeUintegerChecker<uint16_t>())
                            .AddAttribute("Interval",
                                          "Interval between packet sends in seconds.",
                                          TimeValue(Seconds(1.0)),
                                          MakeTimeAccessor(&SenderApplication::m_interval),
                                          MakeTimeChecker());
    return tid;
  }

  SenderApplication();
  virtual ~SenderApplication();

protected:
  virtual void StartApplication(void);
  virtual void StopApplication(void);

private:
  void SendPacket();
  uint32_t m_packetSize;
  Mac48Address m_destMac;
  uint16_t m_port;
  Time m_interval;
  Ptr<Socket> m_socket;
  uint32_t m_packetsSent;
};

SenderApplication::SenderApplication()
    : m_packetSize(1024), m_destMac(Mac48Address("00:00:00:00:00:01")), m_port(9), m_interval(Seconds(1.0)), m_socket(nullptr), m_packetsSent(0)
{
}

SenderApplication::~SenderApplication()
{
  m_socket = nullptr;
}

void SenderApplication::StartApplication(void)
{
  TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
  m_socket = Socket::CreateSocket(GetNode(), tid);
  m_socket->Connect(InetSocketAddress(Ipv4Address::GetAny(), m_port));

  Simulator::ScheduleNow(&SenderApplication::SendPacket, this);
}

void SenderApplication::StopApplication(void)
{
  if (m_socket)
  {
    m_socket->Close();
    m_socket = nullptr;
  }
}

void SenderApplication::SendPacket()
{
  Ptr<Packet> packet = Create<Packet>(m_packetSize);
  m_socket->Send(packet);

  NS_LOG_INFO("At time " << Simulator::Now().GetSeconds() << "s, sender sent packet size " << m_packetSize << " to " << m_destMac << " port " << m_port);
  m_packetsSent++;

  Simulator::Schedule(m_interval, &SenderApplication::SendPacket, this);
}

class ReceiverApplication : public Application
{
public:
  static TypeId GetTypeId(void)
  {
    static TypeId tid = TypeId("ReceiverApplication")
                            .SetParent<Application>()
                            .SetGroupName("Tutorial")
                            .AddConstructor<ReceiverApplication>()
                            .AddAttribute("Port",
                                          "Port on which the receiver listens.",
                                          UintegerValue(9),
                                          MakeUintegerAccessor(&ReceiverApplication::m_port),
                                          MakeUintegerChecker<uint16_t>());
    return tid;
  }

  ReceiverApplication();
  virtual ~ReceiverApplication();

protected:
  virtual void StartApplication(void);
  virtual void StopApplication(void);

private:
  void HandleRead(Ptr<Socket> socket);
  uint16_t m_port;
  Ptr<Socket> m_socket;
  std::map<uint64_t, Time> m_pendingPackets;
};

ReceiverApplication::ReceiverApplication()
    : m_port(9), m_socket(nullptr)
{
}

ReceiverApplication::~ReceiverApplication()
{
  m_socket = nullptr;
}

void ReceiverApplication::StartApplication(void)
{
  TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
  m_socket = Socket::CreateSocket(GetNode(), tid);
  InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), m_port);
  m_socket->Bind(local);
  m_socket->SetRecvCallback(MakeCallback(&ReceiverApplication::HandleRead, this));
}

void ReceiverApplication::StopApplication(void)
{
  if (m_socket)
  {
    m_socket->Close();
    m_socket = nullptr;
  }
}

void ReceiverApplication::HandleRead(Ptr<Socket> socket)
{
  Ptr<Packet> packet;
  Address from;
  while ((packet = socket->RecvFrom(from)))
  {
    Time rxTime = Simulator::Now();
    uint64_t uid = packet->GetUid();

    if (m_pendingPackets.find(uid) == m_pendingPackets.end())
    {
      m_pendingPackets[uid] = rxTime;
      Time delay = rxTime - packet->GetTimestamp();
      NS_LOG_INFO("Received packet UID " << uid << " at " << rxTime.GetSeconds() << "s with delay " << delay.GetSeconds() << "s");
    }
  }
}

static class RegisterThisApplication
{
public:
  RegisterThisApplication()
  {
    ObjectFactory factory;
    factory.SetTypeId("SenderApplication");
    factory.Create()->Unref();
    factory.SetTypeId("ReceiverApplication");
    factory.Create()->Unref();
  }
} registerMyApp;

int main(int argc, char *argv[])
{
  CommandLine cmd(__FILE__);
  cmd.Parse(argc, argv);

  NodeContainer nodes;
  nodes.Create(2);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy;
  phy.SetChannel(channel.Create());

  WifiMacHelper mac;
  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211a);

  NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(5.0),
                                "DeltaY", DoubleValue(0.0),
                                "GridWidth", UintegerValue(2),
                                "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(nodes);

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  uint16_t port = 9;

  ReceiverApplication receiverApp;
  receiverApp.SetAttribute("Port", UintegerValue(port));
  nodes.Get(1)->AddApplication(&receiverApp);
  receiverApp.SetStartTime(Seconds(1.0));
  receiverApp.SetStopTime(Seconds(10.0));

  SenderApplication senderApp;
  senderApp.SetAttribute("PacketSize", UintegerValue(1024));
  senderApp.SetAttribute("DestinationAddress", Mac48AddressValue(interfaces.GetAddress(1)));
  senderApp.SetAttribute("Port", UintegerValue(port));
  senderApp.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  nodes.Get(0)->AddApplication(&senderApp);
  senderApp.SetStartTime(Seconds(2.0));
  senderApp.SetStopTime(Seconds(10.0));

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();
  return 0;
}