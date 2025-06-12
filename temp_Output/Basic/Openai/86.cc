#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiSenderReceiverExample");

// Sender Application
class WifiSenderApp : public Application
{
public:
  WifiSenderApp() : m_socket(0), m_sendEvent(), m_running(false), m_packetsSent(0) {}

  void SetPacketSize(uint32_t size) { m_packetSize = size; }
  void SetRemote(Address address, uint16_t port)
  {
    m_peerAddress = address;
    m_peerPort = port;
  }
  void SetInterval(Time interval) { m_interval = interval; }
  void SetNumPackets(uint32_t n) { m_numPackets = n; }

protected:
  virtual void StartApplication()
  {
    m_running = true;
    m_packetsSent = 0;
    if (!m_socket)
    {
      m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
    }
    InetSocketAddress remote = InetSocketAddress(Ipv4Address::ConvertFrom(m_peerAddress), m_peerPort);
    m_socket->Connect(remote);
    SendPacket();
  }

  virtual void StopApplication()
  {
    m_running = false;
    if (m_sendEvent.IsRunning())
      Simulator::Cancel(m_sendEvent);
    if (m_socket)
    {
      m_socket->Close();
      m_socket = 0;
    }
  }

private:
  void SendPacket()
  {
    Ptr<Packet> packet = Create<Packet>(m_packetSize);
    uint64_t now = Simulator::Now().GetTimeStep();
    packet->AddByteTag(TimeTag(now));
    m_socket->Send(packet);
    ++m_packetsSent;
    if (m_packetsSent < m_numPackets)
    {
      m_sendEvent = Simulator::Schedule(m_interval, &WifiSenderApp::SendPacket, this);
    }
  }

  class TimeTag : public Tag
  {
  public:
    TimeTag() : m_time(0) {}
    TimeTag(uint64_t t) : m_time(t) {}
    static TypeId GetTypeId(void)
    {
      static TypeId tid = TypeId("ns3::WifiSenderApp::TimeTag")
        .SetParent<Tag>()
        .AddConstructor<TimeTag>();
      return tid;
    }
    virtual TypeId GetInstanceTypeId() const { return GetTypeId(); }
    virtual void Serialize(TagBuffer i) const { i.WriteU64(m_time); }
    virtual void Deserialize(TagBuffer i) { m_time = i.ReadU64(); }
    virtual uint32_t GetSerializedSize() const { return 8; }
    virtual void Print(std::ostream &os) const { os << m_time; }
    uint64_t GetTime() const { return m_time; }
  private:
    uint64_t m_time;
  };

  Ptr<Socket> m_socket;
  EventId m_sendEvent;
  bool m_running;
  uint32_t m_packetsSent;
  uint32_t m_packetSize;
  uint32_t m_numPackets;
  Address m_peerAddress;
  uint16_t m_peerPort;
  Time m_interval;
};

// Receiver Application
class WifiReceiverApp : public Application
{
public:
  WifiReceiverApp() : m_socket(0), m_port(0), m_packetsReceived(0), m_totalDelay(Seconds(0)) {}

  void SetPort(uint16_t port)
  {
    m_port = port;
  }

protected:
  virtual void StartApplication()
  {
    m_packetsReceived = 0;
    m_totalDelay = Seconds(0);
    if (!m_socket)
    {
      m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
      InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), m_port);
      m_socket->Bind(local);
    }
    m_socket->SetRecvCallback(MakeCallback(&WifiReceiverApp::HandleRead, this));
  }

  virtual void StopApplication()
  {
    if (m_socket)
    {
      m_socket->Close();
      m_socket = 0;
    }
    NS_LOG_UNCOND("Receiver received " << m_packetsReceived << " packets, average delay: "
                  << (m_packetsReceived > 0 ? m_totalDelay.GetSeconds() / m_packetsReceived : 0) << " s");
  }

private:
  void HandleRead(Ptr<Socket> socket)
  {
    Address from;
    Ptr<Packet> packet = socket->RecvFrom(from);
    WifiSenderApp::TimeTag tag;
    uint64_t txTime = 0;
    if (packet->PeekPacketTag(tag))
    {
      txTime = tag.GetTime();
      Time now = Simulator::Now();
      Time sent = TimeStep(txTime);
      Time delay = now - sent;
      m_totalDelay += delay;
      ++m_packetsReceived;
      NS_LOG_UNCOND("Received packet of " << packet->GetSize()
        << " bytes from " << InetSocketAddress::ConvertFrom(from).GetIpv4()
        << ", delay: " << delay.GetSeconds() << " s");
    }
    else
    {
      NS_LOG_UNCOND("Received packet with no timestamp");
    }
  }

  Ptr<Socket> m_socket;
  uint16_t m_port;
  uint32_t m_packetsReceived;
  Time m_totalDelay;
};

int main(int argc, char *argv[])
{
  uint32_t numPackets = 20;
  uint32_t packetSize = 1024;
  double interval = 0.5;
  uint16_t port = 4444;

  CommandLine cmd;
  cmd.AddValue("numPackets", "Number of packets", numPackets);
  cmd.AddValue("packetSize", "Packet size (bytes)", packetSize);
  cmd.AddValue("interval", "Interval between packets (s)", interval);
  cmd.Parse(argc, argv);

  NodeContainer nodes;
  nodes.Create(2);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
  phy.SetChannel(channel.Create());

  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211b);

  WifiMacHelper mac;
  Ssid ssid = Ssid("wifi-simple");

  mac.SetType("ns3::StaWifiMac",
              "Ssid", SsidValue(ssid));
  NetDeviceContainer staDevices;
  staDevices = wifi.Install(phy, mac, nodes.Get(0));

  mac.SetType("ns3::ApWifiMac",
              "Ssid", SsidValue(ssid));
  NetDeviceContainer apDevices;
  apDevices = wifi.Install(phy, mac, nodes.Get(1));

  MobilityHelper mobility;
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(nodes);

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(NetDeviceContainer(staDevices, apDevices));

  Ptr<WifiSenderApp> senderApp = CreateObject<WifiSenderApp>();
  senderApp->SetPacketSize(packetSize);
  senderApp->SetInterval(Seconds(interval));
  senderApp->SetNumPackets(numPackets);
  senderApp->SetRemote(interfaces.GetAddress(1), port);
  nodes.Get(0)->AddApplication(senderApp);
  senderApp->SetStartTime(Seconds(1.0));
  senderApp->SetStopTime(Seconds(1.0 + numPackets * interval + 2.0));

  Ptr<WifiReceiverApp> receiverApp = CreateObject<WifiReceiverApp>();
  receiverApp->SetPort(port);
  nodes.Get(1)->AddApplication(receiverApp);
  receiverApp->SetStartTime(Seconds(0.5));
  receiverApp->SetStopTime(Seconds(1.0 + numPackets * interval + 3.0));

  Simulator::Stop(Seconds(1.0 + numPackets * interval + 4.0));
  Simulator::Run();
  Simulator::Destroy();
  return 0;
}