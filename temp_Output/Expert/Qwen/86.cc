#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiSenderReceiverSimulation");

class WifiReceiver : public Application
{
public:
  static TypeId GetTypeId(void)
  {
    static TypeId tid = TypeId("WifiReceiver")
                          .SetParent<Application>()
                          .SetGroupName("Tutorial")
                          .AddConstructor<WifiReceiver>()
                          .AddAttribute("Port",
                                        "Listening port for incoming packets",
                                        UintegerValue(9),
                                        MakeUintegerAccessor(&WifiReceiver::m_port),
                                        MakeUintegerChecker<uint16_t>());
    return tid;
  }

  WifiReceiver() : m_socket(nullptr), m_received(0) {}
  virtual ~WifiReceiver() {}

private:
  void StartApplication(void)
  {
    if (!m_socket)
    {
      TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
      m_socket = Socket::CreateSocket(GetNode(), tid);
      InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), m_port);
      m_socket->Bind(local);
      m_socket->SetRecvCallback(MakeCallback(&WifiReceiver::ReceivePacket, this));
    }
  }

  void StopApplication(void)
  {
    if (m_socket)
    {
      m_socket->Close();
      m_socket = nullptr;
    }
  }

  void ReceivePacket(Ptr<Socket> socket)
  {
    Ptr<Packet> packet;
    Address from;
    while ((packet = socket->RecvFrom(from)))
    {
      m_received += packet->GetSize();

      Time rxTime = Now();
      uint8_t buffer[8];
      packet->CopyData(buffer, 8);
      uint64_t txTimestamp = *(uint64_t *)buffer;
      Time delay = rxTime - NanoSeconds(txTimestamp);

      NS_LOG_UNCOND("Received packet of size " << packet->GetSize()
                                              << " bytes at time " << rxTime.As(Time::S)
                                              << " with end-to-end delay " << delay.As(Time::MS));
    }
  }

  Ptr<Socket> m_socket;
  uint16_t m_port;
  uint32_t m_received;
};

class WifiSender : public Application
{
public:
  static TypeId GetTypeId(void)
  {
    static TypeId tid = TypeId("WifiSender")
                          .SetParent<Application>()
                          .SetGroupName("Tutorial")
                          .AddConstructor<WifiSender>()
                          .AddAttribute("RemoteAddress",
                                        "Destination IPv4 Address",
                                        Ipv4AddressValue(),
                                        MakeIpv4AddressAccessor(&WifiSender::m_destAddr),
                                        MakeIpv4AddressChecker())
                          .AddAttribute("RemotePort",
                                        "Destination Port",
                                        UintegerValue(9),
                                        MakeUintegerAccessor(&WifiSender::m_destPort),
                                        MakeUintegerChecker<uint16_t>())
                          .AddAttribute("Interval",
                                        "Interval between packets (in seconds)",
                                        DoubleValue(1.0),
                                        MakeDoubleAccessor(&WifiSender::m_interval),
                                        MakeDoubleChecker<double>())
                          .AddAttribute("PacketSize",
                                        "Size of each packet (bytes)",
                                        UintegerValue(1024),
                                        MakeUintegerAccessor(&WifiSender::m_pktSize),
                                        MakeUintegerChecker<uint32_t>())
                          .AddAttribute("TotalPackets",
                                        "Number of packets to send",
                                        UintegerValue(5),
                                        MakeUintegerAccessor(&WifiSender::m_totalPackets),
                                        MakeUintegerChecker<uint32_t>());
    return tid;
  }

  WifiSender() : m_socket(nullptr), m_packetsSent(0), m_sendEvent() {}
  virtual ~WifiSender() {}

private:
  void StartApplication(void)
  {
    if (!m_socket)
    {
      TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
      m_socket = Socket::CreateSocket(GetNode(), tid);
    }

    m_peerAddress = InetSocketAddress(m_destAddr, m_destPort);
    ScheduleTransmit(Seconds(0.0));
  }

  void StopApplication(void)
  {
    if (m_sendEvent.IsRunning())
    {
      Simulator::Cancel(m_sendEvent);
    }
  }

  void ScheduleTransmit(Time dt)
  {
    m_sendEvent = Simulator::Schedule(dt, &WifiSender::SendPacket, this);
  }

  void SendPacket(void)
  {
    if (m_packetsSent >= m_totalPackets)
    {
      return;
    }

    Ptr<Packet> packet = Create<Packet>(m_pktSize);
    uint64_t timestamp = Now().GetNanoSeconds();
    packet->AddHeader(SequenceNumber64Header(timestamp));

    if (m_socket->SendTo(packet, 0, m_peerAddress) >= 0)
    {
      NS_LOG_UNCOND("Packet of size " << packet->GetSize() << " sent at time "
                                      << Now().As(Time::S) << " to "
                                      << m_destAddr << ":" << m_destPort);
      m_packetsSent++;
    }
    else
    {
      NS_LOG_ERROR("Failed to send packet!");
    }

    ScheduleTransmit(Seconds(m_interval));
  }

  class SequenceNumber64Header : public Header
  {
  public:
    SequenceNumber64Header(uint64_t value = 0) : m_value(value) {}
    uint64_t GetValue(void) const { return m_value; }
    static TypeId GetTypeId(void)
    {
      static TypeId tid = TypeId("SequenceNumber64Header")
                            .SetParent<Header>()
                            .SetGroupName("Applications")
                            .AddConstructor<SequenceNumber64Header>();
      return tid;
    }
    virtual TypeId GetInstanceTypeId(void) const { return GetTypeId(); }
    virtual void Print(std::ostream &os) const { os << m_value; }
    virtual uint32_t GetSerializedSize(void) const { return 8; }
    virtual void Serialize(Buffer::Iterator start) const
    {
      start.WriteHtonU64(m_value);
    }
    virtual uint32_t Deserialize(Buffer::Iterator start)
    {
      m_value = start.ReadNtohU64();
      return 8;
    }

  private:
    uint64_t m_value;
  };

  Ptr<Socket> m_socket;
  InetSocketAddress m_peerAddress;
  Ipv4Address m_destAddr;
  uint16_t m_destPort;
  double m_interval;
  uint32_t m_pktSize;
  uint32_t m_totalPackets;
  uint32_t m_packetsSent;
  EventId m_sendEvent;
};

int main(int argc, char *argv[])
{
  LogComponentEnable("WifiSenderReceiverSimulation", LOG_LEVEL_INFO);

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

  ApplicationContainer receiverApp;
  WifiReceiverHelper receiverHelper;
  receiverApp.Add(receiverHelper.Install(nodes.Get(1)));
  receiverApp.Start(Seconds(1.0));
  receiverApp.Stop(Seconds(10.0));

  ApplicationContainer senderApp;
  WifiSenderHelper senderHelper;
  senderHelper.SetAttribute("RemoteAddress", Ipv4Value(interfaces.GetAddress(1)));
  senderHelper.SetAttribute("RemotePort", UintegerValue(9));
  senderHelper.SetAttribute("Interval", DoubleValue(1.0));
  senderHelper.SetAttribute("PacketSize", UintegerValue(1024));
  senderHelper.SetAttribute("TotalPackets", UintegerValue(5));
  senderApp.Add(senderHelper.Install(nodes.Get(0)));
  senderApp.Start(Seconds(2.0));
  senderApp.Stop(Seconds(10.0));

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  Simulator::Run();
  Simulator::Destroy();
  return 0;
}