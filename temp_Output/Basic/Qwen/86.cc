#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiSenderReceiver");

class SenderApplication : public Application
{
public:
  static TypeId GetTypeId(void)
  {
    static TypeId tid = TypeId("SenderApplication")
      .SetParent<Application>()
      .SetGroupName("Tutorial")
      .AddConstructor<SenderApplication>()
      .AddAttribute("PacketSize", "Size of packets sent in bytes",
                    UintegerValue(1024),
                    MakeUintegerAccessor(&SenderApplication::m_packetSize),
                    MakeUintegerChecker<uint32_t>())
      .AddAttribute("Port", "Destination port.",
                    UintegerValue(9),
                    MakeUintegerAccessor(&SenderApplication::m_port),
                    MakeUintegerChecker<uint16_t>())
      .AddAttribute("Interval", "Interval between packet sends in seconds.",
                    TimeValue(Seconds(1.0)),
                    MakeTimeAccessor(&SenderApplication::m_interval),
                    MakeTimeChecker())
      .AddAttribute("RemoteAddress", "Destination IPv4 Address",
                    Ipv4AddressValue("10.1.1.2"),
                    MakeIpv4AddressAccessor(&SenderApplication::m_remoteAddress),
                    MakeIpv4AddressChecker())
      .AddAttribute("TotalPackets", "Number of packets to send",
                    UintegerValue(5),
                    MakeUintegerAccessor(&SenderApplication::m_totalPackets),
                    MakeUintegerChecker<uint32_t>());
    return tid;
  }

  SenderApplication()
    : m_socket(nullptr),
      m_packetsSent(0)
  {
  }

  virtual ~SenderApplication()
  {
    m_socket = nullptr;
  }

private:
  void StartApplication() override
  {
    m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
    m_socket->Connect(InetSocketAddress(m_remoteAddress, m_port));
    SendPacket();
  }

  void StopApplication() override
  {
    if (m_socket)
    {
      m_socket->Close();
    }
  }

  void SendPacket()
  {
    if (m_packetsSent < m_totalPackets)
    {
      Ptr<Packet> packet = Create<Packet>(m_packetSize);
      m_socket->Send(packet);
      NS_LOG_INFO("At time " << Now().GetSeconds() << "s sent packet " << m_packetsSent + 1 << " to " << m_remoteAddress << ":" << m_port);
      m_packetsSent++;
      Simulator::Schedule(m_interval, &SenderApplication::SendPacket, this);
    }
  }

  uint32_t m_packetSize;
  uint16_t m_port;
  Time m_interval;
  Ipv4Address m_remoteAddress;
  uint32_t m_totalPackets;
  Ptr<Socket> m_socket;
  uint32_t m_packetsSent;
};

class ReceiverApplication : public Application
{
public:
  static TypeId GetTypeId(void)
  {
    static TypeId tid = TypeId("ReceiverApplication")
      .SetParent<Application>()
      .SetGroupName("Tutorial")
      .AddConstructor<ReceiverApplication>()
      .AddAttribute("Port", "Listening port.",
                    UintegerValue(9),
                    MakeUintegerAccessor(&ReceiverApplication::m_port),
                    MakeUintegerChecker<uint16_t>());
    return tid;
  }

  ReceiverApplication()
    : m_socket(nullptr)
  {
  }

  virtual ~ReceiverApplication()
  {
    m_socket = nullptr;
  }

private:
  void StartApplication() override
  {
    m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
    InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), m_port);
    m_socket->Bind(local);
    m_socket->SetRecvCallback(MakeCallback(&ReceiverApplication::HandleRead, this));
    NS_LOG_INFO("Receiver started on port " << m_port);
  }

  void StopApplication() override
  {
    if (m_socket)
    {
      m_socket->Close();
    }
  }

  void HandleRead(Ptr<Socket> socket)
  {
    Ptr<Packet> packet;
    Address from;
    while ((packet = socket->RecvFrom(from)))
    {
      Time rxTime = Now();
      NS_LOG_INFO("Received packet at " << rxTime.GetSeconds() << "s from " << from << ", size: " << packet->GetSize());
      auto it = m_txTimestamps.find(packet->GetUid());
      if (it != m_txTimestamps.end())
      {
        Time delay = rxTime - it->second;
        NS_LOG_INFO("Packet UID " << packet->GetUid() << " delay: " << delay.GetSeconds() << "s");
        m_totalDelay += delay;
        m_packetCount++;
      }
      else
      {
        NS_LOG_WARN("No timestamp found for packet UID " << packet->GetUid());
      }
    }
  }

  void DoDispose() override
  {
    Application::DoDispose();
  }

  uint16_t m_port;
  Ptr<Socket> m_socket;
  std::map<uint64_t, Time> m_txTimestamps;
  Time m_totalDelay{0};
  uint32_t m_packetCount{0};
};

static void PacketSent(Ptr<const Packet> packet)
{
  uint64_t uid = packet->GetUid();
  Time txTime = Now();
  NS_LOG_DEBUG("Recorded TX time for packet UID " << uid << " at " << txTime.GetSeconds() << "s");
  auto receiverApp = DynamicCast<ReceiverApplication>(Simulator::GetContextObject<Ptr<Application>>(1));
  if (receiverApp)
  {
    receiverApp->m_txTimestamps[uid] = txTime;
  }
}

int main(int argc, char *argv[])
{
  LogComponentEnable("WifiSenderReceiver", LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create(2);

  WifiMacHelper wifiMac;
  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211a);
  wifi.SetRemoteStationManager("ns3::ArfRateControl");

  YansWifiPhyHelper wifiPhy;
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  wifiPhy.SetChannel(channel.Create());

  wifiMac.SetType("ns3::AdhocWifiMac");
  NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

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

  ReceiverApplication receiverApp;
  receiverApp.SetAttribute("Port", UintegerValue(9));
  nodes.Get(1)->AddApplication(&receiverApp);
  receiverApp.SetStartTime(Seconds(1.0));
  receiverApp.SetStopTime(Seconds(10.0));

  SenderApplication senderApp;
  senderApp.SetAttribute("Port", UintegerValue(9));
  senderApp.SetAttribute("RemoteAddress", Ipv4AddressValue(interfaces.GetAddress(1)));
  senderApp.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  senderApp.SetAttribute("TotalPackets", UintegerValue(5));
  senderApp.SetAttribute("PacketSize", UintegerValue(1024));
  nodes.Get(0)->AddApplication(&senderApp);
  senderApp.SetStartTime(Seconds(2.0));
  senderApp.SetStopTime(Seconds(10.0));

  Config::ConnectWithoutContext("/NodeList/*/ApplicationList/*/$SenderApplication/Tx", MakeCallback(&PacketSent));

  Simulator::Run();
  Simulator::Destroy();

  return 0;
}