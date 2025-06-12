#include "ns3/aodv-helper.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/applications-module.h"
#include "ns3/packet.h"
#include <vector>
#include <numeric>
#include <algorithm>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("AodvAdhocRandomPdr");

struct FlowStats
{
  uint32_t txPackets = 0;
  uint32_t rxPackets = 0;
  uint32_t lostPackets = 0;
  double delaySum = 0.0;
  uint32_t delayCount = 0;
};

std::map<uint64_t, FlowStats> flowStats; // key: flow id

void
TxTrace(Ptr<const Packet> packet)
{
  // Use packet UID as flow ID
  uint64_t uid = packet->GetUid();
  flowStats[uid].txPackets++;
}

void
RxTrace(Ptr<const Packet> packet, const Address &address)
{
  uint64_t uid = packet->GetUid();
  flowStats[uid].rxPackets++;
}

void
RxDelayTrace(Ptr<const Packet> packet, const Address &address, const Time &recvTime)
{
  uint64_t uid = packet->GetUid();
  Time sendTime;
  if (packet->PeekPacketTag(sendTime))
    {
      double delay = (recvTime - sendTime).GetSeconds();
      flowStats[uid].delaySum += delay;
      flowStats[uid].delayCount++;
    }
}

class SendTimeTag : public Tag
{
public:
  SendTimeTag() {}
  SendTimeTag(Time sendTime) : m_sendTime(sendTime) {}

  static TypeId GetTypeId(void)
  {
    static TypeId tid = TypeId("SendTimeTag")
                            .SetParent<Tag>()
                            .AddConstructor<SendTimeTag>();
    return tid;
  }
  virtual TypeId GetInstanceTypeId(void) const { return GetTypeId(); }
  virtual void Serialize(TagBuffer i) const { i.WriteDouble(m_sendTime.GetSeconds()); }
  virtual void Deserialize(TagBuffer i) { m_sendTime = Seconds(i.ReadDouble()); }
  virtual uint32_t GetSerializedSize(void) const { return sizeof(double); }
  virtual void Print(std::ostream &os) const { os << "t=" << m_sendTime.GetSeconds(); }
  void SetSendTime(Time t) { m_sendTime = t; }
  Time GetSendTime() const { return m_sendTime; }

private:
  Time m_sendTime;
};

class CustomUdpClient : public Application
{
public:
  CustomUdpClient();
  virtual ~CustomUdpClient();

  void Setup(Address address, uint16_t port, uint32_t maxPackets, DataRate dataRate, uint32_t packetSize, Time startTime);

private:
  virtual void StartApplication(void);
  virtual void StopApplication(void);
  void ScheduleNextTx(void);
  void SendPacket(void);

  Ptr<Socket> m_socket;
  Address m_peerAddress;
  uint16_t m_peerPort;
  EventId m_sendEvent;
  uint32_t m_sent;
  uint32_t m_maxPackets;
  DataRate m_dataRate;
  uint32_t m_packetSize;
  bool m_running;
};

CustomUdpClient::CustomUdpClient()
    : m_socket(0),
      m_peerPort(0),
      m_sendEvent(),
      m_sent(0),
      m_maxPackets(0),
      m_dataRate(0),
      m_packetSize(0),
      m_running(false)
{
}
CustomUdpClient::~CustomUdpClient()
{
  m_socket = 0;
}

void
CustomUdpClient::Setup(Address address, uint16_t port, uint32_t maxPackets, DataRate dataRate, uint32_t packetSize, Time startTime)
{
  m_peerAddress = address;
  m_peerPort = port;
  m_maxPackets = maxPackets;
  m_dataRate = dataRate;
  m_packetSize = packetSize;
}

void
CustomUdpClient::StartApplication()
{
  m_running = true;
  m_sent = 0;
  if (!m_socket)
    {
      m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
      m_socket->Connect(InetSocketAddress(Ipv4Address::ConvertFrom(m_peerAddress), m_peerPort));
    }
  SendPacket();
}

void
CustomUdpClient::StopApplication()
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
CustomUdpClient::SendPacket()
{
  if (!m_running)
    return;

  Ptr<Packet> packet = Create<Packet>(m_packetSize);
  SendTimeTag st;
  st.SetSendTime(Simulator::Now());
  packet->AddPacketTag(st);

  m_socket->Send(packet);
  m_sent++;
  if (m_sent < m_maxPackets)
    {
      ScheduleNextTx();
    }
}

void
CustomUdpClient::ScheduleNextTx()
{
  if (m_running)
    {
      Time tNext = Seconds(static_cast<double>(m_packetSize * 8) / static_cast<double>(m_dataRate.GetBitRate()));
      m_sendEvent = Simulator::Schedule(tNext, &CustomUdpClient::SendPacket, this);
    }
}

// Global counters
uint32_t totalRx = 0;
double totalDelay = 0.0;
uint32_t delayCount = 0;

void ReceivePacket(Ptr<Socket> socket)
{
  Address from;
  while (Ptr<Packet> packet = socket->RecvFrom(from))
    {
      ++totalRx;
      SendTimeTag tag;
      if (packet->PeekPacketTag(tag))
        {
          double delay = (Simulator::Now() - tag.GetSendTime()).GetSeconds();
          totalDelay += delay;
          ++delayCount;
        }
    }
}

int main(int argc, char *argv[])
{
  uint32_t numNodes = 10;
  double simTime = 30.0;
  double areaSize = 500.0;

  CommandLine cmd;
  cmd.Parse(argc, argv);

  // Seed
  SeedManager::SetSeed(12345);

  // Create nodes
  NodeContainer nodes;
  nodes.Create(numNodes);

  // Mobility: RandomWaypointMobilityModel
  MobilityHelper mobility;
  mobility.SetMobilityModel("ns3::RandomWaypointMobilityModel",
                            "Speed", StringValue("ns3::UniformRandomVariable[Min=2.0|Max=10.0]"),
                            "Pause", StringValue("ns3::ConstantRandomVariable[Constant=0.5]"),
                            "PositionAllocator", StringValue("ns3::RandomRectanglePositionAllocator[MinX=0.0|MinY=0.0|MaxX=500.0|MaxY=500.0]"));
  mobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "MaxX", DoubleValue(areaSize),
                                "MaxY", DoubleValue(areaSize));

  mobility.Install(nodes);

  // WiFi (ad-hoc)
  WifiHelper wifi;
  wifi.SetStandard(WIFI_PHY_STANDARD_80211b);
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
  phy.SetPcapDataLinkType(YansWifiPhyHelper::DLT_IEEE802_11_RADIO);
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  phy.SetChannel(channel.Create());

  WifiMacHelper mac;
  mac.SetType("ns3::AdhocWifiMac");
  NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

  // Internet stack with AODV
  AodvHelper aodv;
  InternetStackHelper internet;
  internet.SetRoutingHelper(aodv);
  internet.Install(nodes);

  // Assign IP addresses
  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.1.0.0", "255.255.0.0");
  Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

  // UDP applications (random pairs, random times)
  uint16_t port = 4000;
  Ptr<UniformRandomVariable> rand = CreateObject<UniformRandomVariable>();
  std::vector<Ptr<CustomUdpClient>> clients;

  for (uint32_t i = 0; i < numNodes; ++i)
    {
      // Pick a random destination (not self)
      uint32_t dst;
      do
        {
          dst = rand->GetInteger(0, numNodes - 1);
        }
      while (dst == i);

      // UDP Server on dst
      Ptr<Socket> recvSocket = Socket::CreateSocket(nodes.Get(dst), UdpSocketFactory::GetTypeId());
      InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), port);
      recvSocket->Bind(local);
      recvSocket->SetRecvCallback(MakeCallback(&ReceivePacket));

      // UDP Client on src
      Address remoteAddr = interfaces.GetAddress(dst);
      uint32_t maxPackets = 30;
      uint32_t pktSize = 512;
      DataRate dataRate = DataRate("128kbps");
      double startTime = rand->GetValue(1.0, simTime / 2);
      double stopTime = simTime;

      Ptr<CustomUdpClient> client = CreateObject<CustomUdpClient>();
      client->Setup(remoteAddr, port, maxPackets, dataRate, pktSize, Seconds(startTime));
      nodes.Get(i)->AddApplication(client);
      client->SetStartTime(Seconds(startTime));
      client->SetStopTime(Seconds(stopTime));
      clients.push_back(client);
    }

  // Enable pcap tracing
  phy.EnablePcapAll("aodv-adhoc");

  Simulator::Stop(Seconds(simTime));
  Simulator::Run();

  // Calculate PDR and average delay
  uint32_t txPackets = 0;
  for (auto &c : clients)
    {
      txPackets += 30; // as set above
    }
  double pdr = 0.0;
  if (txPackets > 0)
    pdr = (double)totalRx / (double)txPackets * 100.0;
  double avgDelay = (delayCount > 0) ? (totalDelay / delayCount) : 0.0;

  std::cout << "========== Simulation Results ==========\n";
  std::cout << "Total Packets Sent:  " << txPackets << std::endl;
  std::cout << "Total Packets Recv:  " << totalRx << std::endl;
  std::cout << "Packet Delivery Ratio: " << pdr << " %\n";
  std::cout << "Average End-to-End Delay: " << avgDelay << " s\n";
  std::cout << "========================================\n";

  Simulator::Destroy();
  return 0;
}