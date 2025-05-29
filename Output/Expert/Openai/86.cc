#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-address-helper.h"

using namespace ns3;

class WiFiSenderApp : public Application
{
public:
  WiFiSenderApp();
  virtual ~WiFiSenderApp();

  void SetRemote(Address ip, uint16_t port) { m_peerAddress = ip; m_peerPort = port; }
  void SetPacketSize(uint32_t size) { m_pktSize = size; }
  void SetNumPackets(uint32_t n) { m_numPkts = n; }
  void SetInterval(Time t) { m_interval = t; }

protected:
  virtual void StartApplication() override;
  virtual void StopApplication() override;

private:
  void SendPacket();

  Ptr<Socket> m_socket;
  Address m_peerAddress;
  uint16_t m_peerPort;
  uint32_t m_pktSize;
  uint32_t m_numPkts;
  uint32_t m_sent;
  Time m_interval;
  EventId m_sendEvent;
};

WiFiSenderApp::WiFiSenderApp()
  : m_socket(nullptr),
    m_peerPort(0),
    m_pktSize(1024),
    m_numPkts(10),
    m_sent(0),
    m_interval(Seconds(1.0)) {}

WiFiSenderApp::~WiFiSenderApp()
{
  m_socket = nullptr;
}

void WiFiSenderApp::StartApplication()
{
  if (!m_socket)
  {
    m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
    m_socket->Connect(InetSocketAddress(InetSocketAddress::ConvertFrom(m_peerAddress).GetIpv4(), m_peerPort));
  }
  m_sent = 0;
  SendPacket();
}

void WiFiSenderApp::StopApplication()
{
  if (m_socket)
  {
    m_socket->Close();
    m_socket = nullptr;
  }
  Simulator::Cancel(m_sendEvent);
}

void WiFiSenderApp::SendPacket()
{
  Ptr<Packet> packet = Create<Packet>(m_pktSize);
  Time sendTime = Simulator::Now();
  packet->AddByteTag(TimeTag(sendTime.GetNanoSeconds()));
  m_socket->Send(packet);
  ++m_sent;
  if (m_sent < m_numPkts)
  {
    m_sendEvent = Simulator::Schedule(m_interval, &WiFiSenderApp::SendPacket, this);
  }
}

// Tag for time stamping packets
class TimeTag : public Tag
{
public:
  TimeTag() : m_timeNano(0) {}
  TimeTag(uint64_t t) : m_timeNano(t) {}
  static TypeId GetTypeId()
  {
    static TypeId tid = TypeId("TimeTag")
      .SetParent<Tag>()
      .AddConstructor<TimeTag>();
    return tid;
  }
  virtual TypeId GetInstanceTypeId() const override { return GetTypeId(); }
  virtual void Serialize(TagBuffer i) const override { i.WriteU64(m_timeNano); }
  virtual void Deserialize(TagBuffer i) override { m_timeNano = i.ReadU64(); }
  virtual uint32_t GetSerializedSize() const override { return 8; }
  virtual void Print(std::ostream &os) const override { os << "t=" << m_timeNano; }
  void SetTime(uint64_t t) { m_timeNano = t; }
  uint64_t GetTime() const { return m_timeNano; }
private:
  uint64_t m_timeNano;
};

class WiFiReceiverApp : public Application
{
public:
  WiFiReceiverApp();
  virtual ~WiFiReceiverApp();

  void SetPort(uint16_t port) { m_listenPort = port; }

protected:
  virtual void StartApplication() override;
  virtual void StopApplication() override;

private:
  void HandleRead(Ptr<Socket> socket);

  Ptr<Socket> m_socket;
  uint16_t m_listenPort;
  uint32_t m_rxPkts;
  Time m_totalDelay;
};

WiFiReceiverApp::WiFiReceiverApp()
  : m_socket(nullptr),
    m_listenPort(0),
    m_rxPkts(0),
    m_totalDelay(Seconds(0.0))
{}

WiFiReceiverApp::~WiFiReceiverApp()
{
  m_socket = nullptr;
}

void WiFiReceiverApp::StartApplication()
{
  if (!m_socket)
  {
    m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
    InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), m_listenPort);
    m_socket->Bind(local);
    m_socket->SetRecvCallback(MakeCallback(&WiFiReceiverApp::HandleRead, this));
  }
  m_rxPkts = 0;
  m_totalDelay = Seconds(0.0);
}

void WiFiReceiverApp::StopApplication()
{
  if (m_socket)
  {
    m_socket->Close();
    m_socket = nullptr;
  }
}

void WiFiReceiverApp::HandleRead(Ptr<Socket> socket)
{
  Ptr<Packet> packet;
  Address from;
  while ((packet = socket->RecvFrom(from)))
  {
    TimeTag tag;
    bool found = packet->PeekPacketTag(tag);
    Time now = Simulator::Now();
    if (found)
    {
      Time sent = NanoSeconds(tag.GetTime());
      Time delay = now - sent;
      m_totalDelay += delay;
      ++m_rxPkts;
      std::cout << "At " << now.GetSeconds() << "s: Received packet of "
                << packet->GetSize() << " bytes from "
                << InetSocketAddress::ConvertFrom(from).GetIpv4()
                << ", delay=" << delay.GetMilliSeconds() << " ms"
                << std::endl;
    }
    else
    {
      std::cout << "At " << now.GetSeconds() << "s: Received packet (no time tag)"
                << std::endl;
    }
  }
}

int main(int argc, char *argv[])
{
  uint32_t packetSize = 1024;
  uint32_t numPackets = 20;
  double interval = 0.5; // seconds
  uint16_t port = 50000;

  CommandLine cmd(__FILE__);
  cmd.AddValue("packetSize", "Size of each packet sent", packetSize);
  cmd.AddValue("numPackets", "Number of packets to send", numPackets);
  cmd.AddValue("interval", "Interval between packets (seconds)", interval);
  cmd.AddValue("port", "UDP destination port", port);
  cmd.Parse(argc, argv);

  NodeContainer wifiStaNodes, wifiApNode;
  wifiStaNodes.Create(1);
  wifiApNode.Create(1);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
  phy.SetChannel(channel.Create());

  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211g);
  wifi.SetRemoteStationManager("ns3::AarfWifiManager");

  WifiMacHelper mac;
  Ssid ssid = Ssid("ns3-wifi-demo");
  mac.SetType("ns3::StaWifiMac",
              "Ssid", SsidValue(ssid),
              "ActiveProbing", BooleanValue(false));

  NetDeviceContainer staDevices;
  staDevices = wifi.Install(phy, mac, wifiStaNodes);

  mac.SetType("ns3::ApWifiMac",
              "Ssid", SsidValue(ssid));
  NetDeviceContainer apDevices;
  apDevices = wifi.Install(phy, mac, wifiApNode);

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                               "MinX", DoubleValue(0.0),
                               "MinY", DoubleValue(0.0),
                               "DeltaX", DoubleValue(5.0),
                               "DeltaY", DoubleValue(5.0),
                               "GridWidth", UintegerValue(2),
                               "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(wifiStaNodes);
  mobility.Install(wifiApNode);

  InternetStackHelper stack;
  stack.Install(wifiStaNodes);
  stack.Install(wifiApNode);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer staIfs = address.Assign(staDevices);
  Ipv4InterfaceContainer apIfs = address.Assign(apDevices);

  Ptr<WiFiSenderApp> sender = CreateObject<WiFiSenderApp>();
  sender->SetPacketSize(packetSize);
  sender->SetNumPackets(numPackets);
  sender->SetInterval(Seconds(interval));
  sender->SetRemote(apIfs.GetAddress(0), port);

  Ptr<WiFiReceiverApp> receiver = CreateObject<WiFiReceiverApp>();
  receiver->SetPort(port);

  wifiStaNodes.Get(0)->AddApplication(sender);
  wifiApNode.Get(0)->AddApplication(receiver);

  sender->SetStartTime(Seconds(1.0));
  sender->SetStopTime(Seconds(1.0 + numPackets * interval + 1.0));
  receiver->SetStartTime(Seconds(0.0));
  receiver->SetStopTime(Seconds(1.0 + numPackets * interval + 2.0));

  Simulator::Stop(Seconds(1.0 + numPackets * interval + 2.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}