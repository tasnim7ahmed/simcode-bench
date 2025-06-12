#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiSenderReceiverExample");

// Custom Sender Application
class WifiSenderApp : public Application
{
public:
  WifiSenderApp()
      : m_socket(0),
        m_peer(),
        m_packetSize(1000),
        m_nPackets(10),
        m_interval(Seconds(1.0)),
        m_sent(0)
  {
  }

  static TypeId GetTypeId(void)
  {
    static TypeId tid = TypeId("WifiSenderApp")
                            .SetParent<Application>()
                            .SetGroupName("Tutorial")
                            .AddConstructor<WifiSenderApp>()
                            .AddAttribute("PacketSize", "Size of packets generated",
                                          UintegerValue(1000),
                                          MakeUintegerAccessor(&WifiSenderApp::m_packetSize),
                                          MakeUintegerChecker<uint32_t>())
                            .AddAttribute("NumPackets", "Number of packets to send",
                                          UintegerValue(10),
                                          MakeUintegerAccessor(&WifiSenderApp::m_nPackets),
                                          MakeUintegerChecker<uint32_t>())
                            .AddAttribute("Interval", "Interval between packets",
                                          TimeValue(Seconds(1.0)),
                                          MakeTimeAccessor(&WifiSenderApp::m_interval),
                                          MakeTimeChecker())
                            .AddAttribute("RemoteAddress", "The destination address",
                                          AddressValue(),
                                          MakeAddressAccessor(&WifiSenderApp::m_peer),
                                          MakeAddressChecker())
                            .AddAttribute("RemotePort", "The destination port",
                                          UintegerValue(9),
                                          MakeUintegerAccessor(&WifiSenderApp::m_peerPort),
                                          MakeUintegerChecker<uint16_t>());
    return tid;
  }

  void SetPacketSize(uint32_t size) { m_packetSize = size; }
  void SetNumPackets(uint32_t num) { m_nPackets = num; }
  void SetInterval(Time interval) { m_interval = interval; }
  void SetRemote(Address address, uint16_t port)
  {
    m_peer = address;
    m_peerPort = port;
  }

protected:
  virtual void StartApplication()
  {
    if (!m_socket)
    {
      m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
      m_socket->Connect(InetSocketAddress(InetSocketAddress::ConvertFrom(m_peer).GetIpv4(), m_peerPort));
    }
    m_sent = 0;
    SendPacket();
  }

  virtual void StopApplication()
  {
    if (m_socket)
    {
      m_socket->Close();
      m_socket = 0;
    }
  }

  void SendPacket()
  {
    Ptr<Packet> packet = Create<Packet>(m_packetSize);
    // Add Timestamp
    uint64_t now_us = Simulator::Now().GetMicroSeconds();
    packet->AddHeader(TimeHeader(now_us));
    m_socket->Send(packet);

    ++m_sent;
    if (m_sent < m_nPackets)
    {
      Simulator::Schedule(m_interval, &WifiSenderApp::SendPacket, this);
    }
  }

  // Helper header for timestamp
  class TimeHeader : public Header
  {
  public:
    TimeHeader() : m_ts(0) {}
    TimeHeader(uint64_t ts) : m_ts(ts) {}
    static TypeId GetTypeId()
    {
      static TypeId tid = TypeId("TimeHeader")
                              .SetParent<Header>()
                              .AddConstructor<TimeHeader>();
      return tid;
    }
    virtual TypeId GetInstanceTypeId() const { return GetTypeId(); }
    virtual void Serialize(Buffer::Iterator start) const
    {
      start.WriteHtonU64(m_ts);
    }
    virtual uint32_t Deserialize(Buffer::Iterator start)
    {
      m_ts = start.ReadNtohU64();
      return GetSerializedSize();
    }
    virtual uint32_t GetSerializedSize() const { return sizeof(uint64_t); }
    virtual void Print(std::ostream &os) const
    {
      os << "Timestamp=" << m_ts;
    }
    uint64_t GetTs() const { return m_ts; }

  private:
    uint64_t m_ts;
  };

private:
  Ptr<Socket> m_socket;
  Address m_peer;
  uint16_t m_peerPort;
  uint32_t m_packetSize;
  uint32_t m_nPackets;
  Time m_interval;
  uint32_t m_sent;
};

// Custom Receiver Application
class WifiReceiverApp : public Application
{
public:
  WifiReceiverApp() : m_socket(0), m_port(9), m_received(0), m_totalDelay(0) {}

  static TypeId GetTypeId(void)
  {
    static TypeId tid =
        TypeId("WifiReceiverApp")
            .SetParent<Application>()
            .SetGroupName("Tutorial")
            .AddConstructor<WifiReceiverApp>()
            .AddAttribute("Port", "Listening port",
                          UintegerValue(9),
                          MakeUintegerAccessor(&WifiReceiverApp::m_port),
                          MakeUintegerChecker<uint16_t>());
    return tid;
  }

  void SetPort(uint16_t port) { m_port = port; }

protected:
  virtual void StartApplication()
  {
    if (!m_socket)
    {
      m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
      InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), m_port);
      m_socket->Bind(local);
      m_socket->SetRecvCallback(MakeCallback(&WifiReceiverApp::HandleRead, this));
    }
    m_received = 0;
    m_totalDelay = Time(0);
  }

  virtual void StopApplication()
  {
    if (m_socket)
    {
      m_socket->Close();
      m_socket = 0;
    }
    if (m_received > 0)
    {
      NS_LOG_UNCOND("Receiver received " << m_received << " packets, average delay = " << (m_totalDelay / m_received).GetMilliSeconds() << " ms");
    }
  }

  void HandleRead(Ptr<Socket> socket)
  {
    Address from;
    Ptr<Packet> packet;
    while ((packet = socket->RecvFrom(from)))
    {
      // Remove header and get timestamp
      WifiSenderApp::TimeHeader timeHeader;
      packet->PeekHeader(timeHeader);
      packet->RemoveHeader(timeHeader);
      uint64_t ts_us = timeHeader.GetTs();
      Time sent = MicroSeconds(ts_us);
      Time delay = Simulator::Now() - sent;
      m_totalDelay += delay;
      ++m_received;
      NS_LOG_UNCOND("Packet received at " << Simulator::Now().GetSeconds() << "s, delay = " << delay.GetMilliSeconds() << " ms, size = " << packet->GetSize() << " bytes");
    }
  }

private:
  Ptr<Socket> m_socket;
  uint16_t m_port;
  uint32_t m_received;
  Time m_totalDelay;
};

int main(int argc, char *argv[])
{
  uint32_t packetSize = 1024;
  uint32_t numPackets = 20;
  double interval = 0.5;
  uint16_t port = 5000;
  double simulationTime = 12.0;

  CommandLine cmd;
  cmd.AddValue("packetSize", "Packet size in bytes", packetSize);
  cmd.AddValue("numPackets", "Number of packets to transmit", numPackets);
  cmd.AddValue("interval", "Interval between packets [s]", interval);
  cmd.AddValue("port", "UDP port", port);
  cmd.AddValue("simulationTime", "Simulation duration [s]", simulationTime);
  cmd.Parse(argc, argv);

  NodeContainer wifiNodes;
  wifiNodes.Create(2);

  // Wi-Fi configuration
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
  phy.SetChannel(channel.Create());

  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211g);
  wifi.SetRemoteStationManager("ns3::AarfWifiManager");

  WifiMacHelper mac;
  Ssid ssid = Ssid("simple-wifi");
  mac.SetType("ns3::StaWifiMac",
              "Ssid", SsidValue(ssid));

  NetDeviceContainer staDevices;
  staDevices.Add(wifi.Install(phy, mac, wifiNodes.Get(0)));

  mac.SetType("ns3::ApWifiMac",
              "Ssid", SsidValue(ssid));

  NetDeviceContainer apDevices;
  apDevices.Add(wifi.Install(phy, mac, wifiNodes.Get(1)));

  // Mobility
  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
  positionAlloc->Add(Vector(0.0, 0.0, 0.0));
  positionAlloc->Add(Vector(5.0, 0.0, 0.0));
  mobility.SetPositionAllocator(positionAlloc);
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(wifiNodes);

  // Internet stack
  InternetStackHelper stack;
  stack.Install(wifiNodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(NetDeviceContainer(staDevices.Get(0), apDevices.Get(0)));

  // Install Receiver (AP)
  Ptr<WifiReceiverApp> receiverApp = CreateObject<WifiReceiverApp>();
  receiverApp->SetAttribute("Port", UintegerValue(port));
  wifiNodes.Get(1)->AddApplication(receiverApp);
  receiverApp->SetStartTime(Seconds(0.0));
  receiverApp->SetStopTime(Seconds(simulationTime));

  // Install Sender (STA)
  Ptr<WifiSenderApp> senderApp = CreateObject<WifiSenderApp>();
  senderApp->SetAttribute("PacketSize", UintegerValue(packetSize));
  senderApp->SetAttribute("NumPackets", UintegerValue(numPackets));
  senderApp->SetAttribute("Interval", TimeValue(Seconds(interval)));
  senderApp->SetAttribute("RemoteAddress", AddressValue(interfaces.GetAddress(1)));
  senderApp->SetAttribute("RemotePort", UintegerValue(port));
  wifiNodes.Get(0)->AddApplication(senderApp);
  senderApp->SetStartTime(Seconds(1.0));
  senderApp->SetStopTime(Seconds(simulationTime));

  Simulator::Stop(Seconds(simulationTime + 1.0));
  Simulator::Run();
  Simulator::Destroy();
  return 0;
}