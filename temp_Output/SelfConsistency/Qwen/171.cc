#include "ns3/aodv-module.h"
#include "ns3/core-module.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("AodvAdHocSimulation");

class UdpPacketCollector : public Application
{
public:
  UdpPacketCollector();
  virtual ~UdpPacketCollector();

  static TypeId GetTypeId(void);
  void SetRemote(Address address);

private:
  virtual void StartApplication(void);
  virtual void StopApplication(void);
  void HandleRead(Ptr<Socket> socket);

  Ptr<Socket> m_socket;
  Address m_peer;
  uint64_t m_received;
  Time m_totalDelay;
};

TypeId
UdpPacketCollector::GetTypeId(void)
{
  static TypeId tid = TypeId("UdpPacketCollector")
                          .SetParent<Application>()
                          .AddConstructor<UdpPacketCollector>();
  return tid;
}

UdpPacketCollector::UdpPacketCollector()
    : m_socket(0), m_received(0), m_totalDelay(0)
{
}

UdpPacketCollector::~UdpPacketCollector()
{
  m_socket = 0;
}

void UdpPacketCollector::SetRemote(Address address)
{
  m_peer = address;
}

void UdpPacketCollector::StartApplication(void)
{
  if (!m_socket)
  {
    TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
    m_socket = Socket::CreateSocket(GetNode(), tid);
    InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), 9);
    m_socket->Bind(local);
    m_socket->Connect(m_peer);
  }

  m_socket->SetRecvCallback(MakeCallback(&UdpPacketCollector::HandleRead, this));
}

void UdpPacketCollector::StopApplication(void)
{
  if (m_socket)
  {
    m_socket->Close();
    m_socket = nullptr;
  }
}

void UdpPacketCollector::HandleRead(Ptr<Socket> socket)
{
  Ptr<Packet> packet;
  Address from;
  while ((packet = socket->RecvFrom(from)))
  {
    if (InetSocketAddress::IsMatchingType(from))
    {
      Ipv4Address senderIp = InetSocketAddress::ConvertFrom(from).GetIpv4();
      if (senderIp == Ipv4Address::GetBroadcast())
        continue;

      SequenceNumberTag tag;
      if (packet->FindFirstOf(tag))
      {
        Time rxTime = Simulator::Now();
        Time txTime = Seconds(tag.Get().GetValue() / 1e9);
        Time delay = rxTime - txTime;
        m_totalDelay += delay;
        m_received++;
        NS_LOG_DEBUG("Received packet " << tag.Get().GetValue() << ", delay: " << delay.As(Time::MS));
      }
    }
  }
}

int main(int argc, char *argv[])
{
  CommandLine cmd(__FILE__);
  cmd.Parse(argc, argv);

  Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue("2200"));

  NodeContainer nodes;
  nodes.Create(10);

  YansWifiPhyHelper wifiPhy;
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
  wifiPhy.SetChannel(wifiChannel.Create());

  WifiMacHelper wifiMac;
  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211a);
  wifi.SetRemoteStationManager("ns3::ArfWifiManager");

  wifiMac.SetType("ns3::AdhocWifiMac");
  NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(100.0),
                                "DeltaY", DoubleValue(100.0),
                                "GridWidth", UintegerValue(5),
                                "LayoutType", StringValue("RowFirst"));

  mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                            "Bounds", RectangleValue(Rectangle(0, 500, 0, 500)));
  mobility.Install(nodes);

  AodvHelper aodv;
  Ipv4ListRoutingHelper list;
  list.Add(aodv, 100);

  InternetStackHelper internet;
  internet.SetRoutingHelper(list);
  internet.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.0.0.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  uint16_t port = 9;

  ApplicationContainer sinks;
  std::vector<Ptr<UdpPacketCollector>> collectors(10);

  for (uint32_t i = 0; i < nodes.GetN(); ++i)
  {
    auto collectorApp = CreateObject<UdpPacketCollector>();
    nodes.Get(i)->AddApplication(collectorApp);
    collectorApp->SetStartTime(Seconds(0.0));
    collectorApp->SetStopTime(Seconds(30.0));
    collectors[i] = collectorApp;
  }

  UniformRandomVariable startTimeRng;
  startTimeRng.SetAttribute("Min", DoubleValue(1.0));
  startTimeRng.SetAttribute("Max", DoubleValue(25.0));

  for (uint32_t i = 0; i < nodes.GetN(); ++i)
  {
    for (uint32_t j = 0; j < nodes.GetN(); ++j)
    {
      if (i != j)
      {
        Ptr<Socket> sourceSocket = Socket::CreateSocket(nodes.Get(i), TypeId::LookupByName("ns3::UdpSocketFactory"));
        InetSocketAddress destAddr(interfaces.GetAddress(j), port);
        sourceSocket->Connect(destAddr);

        class UdpTrafficSender : public Application
        {
        public:
          UdpTrafficSender(Ptr<Socket> socket, Time interval, uint32_t size)
              : m_socket(socket), m_interval(interval), m_size(size), m_seq(0) {}
          virtual void StartApplication()
          {
            ScheduleTransmit(Seconds(startTimeRng.GetValue()));
          }
          virtual void StopApplication() {}

        private:
          void ScheduleTransmit(Time t)
          {
            Simulator::Schedule(t, &UdpTrafficSender::SendPacket, this);
          }

          void SendPacket()
          {
            Ptr<Packet> p = Create<Packet>(m_size);
            SequenceNumberTag tag;
            tag.Get() = SequenceNumber32((Simulator::Now().GetNanoSeconds() & 0x7FFFFFFF));
            p->AddPacketTag(tag);
            m_socket->Send(p);
            ScheduleTransmit(Seconds(startTimeRng.GetValue()));
          }

          Ptr<Socket> m_socket;
          Time m_interval;
          uint32_t m_size;
          uint32_t m_seq;
        };

        Simulator::ScheduleNow(&UdpTrafficSender::StartApplication,
                               new UdpTrafficSender(sourceSocket, MilliSeconds(100), 512));
      }
    }
  }

  wifiPhy.EnablePcapAll("aodv_adhoc_traffic");

  Simulator::Stop(Seconds(30.0));

  Simulator::Run();

  double totalPacketsSent = 0;
  double totalPacketsReceived = 0;
  double totalDelay = 0;

  for (auto &collector : collectors)
  {
    totalPacketsReceived += collector->m_received;
    totalDelay += collector->m_totalDelay.GetMilliSeconds();
  }

  // This is an approximation since we don't track sent packets per destination
  totalPacketsSent = 9 * 10 * (25.0 / 0.1); // approximated by rate * duration * flows

  double pdr = totalPacketsSent > 0 ? (totalPacketsReceived / totalPacketsSent) : 0;
  double avgDelay = totalPacketsReceived > 0 ? (totalDelay / totalPacketsReceived) : 0;

  std::cout.precision(3);
  std::cout << "Packet Delivery Ratio: " << std::fixed << pdr * 100 << "% (" 
            << totalPacketsReceived << "/" << totalPacketsSent << ")" << std::endl;
  std::cout << "Average End-to-End Delay: " << avgDelay << " ms" << std::endl;

  Simulator::Destroy();
  return 0;
}